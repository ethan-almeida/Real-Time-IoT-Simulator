#!/usr/bin/env python3
import socket
import ssl
import threading
import time
import subprocess
import os
import tempfile
import sys
from datetime import datetime

class MITMProxy:
    def __init__(self, target_host, target_port):
        self.target_host = target_host
        self.target_port = target_port
        self.cert_file = None
        self.key_file = None
        self.proxy_socket = None
        self.running = False
        
    def create_intercept_cert(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            self.key_file = os.path.join(temp_dir, "proxy.key")
            self.cert_file = os.path.join(temp_dir, "proxy.crt")
            try:
                subprocess.run([
                    "openssl", "genrsa", "-out", self.key_file, "2048"
                ], check=True, capture_output=True)

                subprocess.run([
                    "openssl", "req", "-new", "-x509", "-key", self.key_file,
                    "-out", self.cert_file, "-days", "1", "-subj",
                    f"/C=US/ST=Test/L=Test/O=Test/CN={self.target_host}"
                ], check=True, capture_output=True)
                
                return True
            except subprocess.CalledProcessError:
                return False
    
    def start_proxy(self, listen_port):
        if not self.create_intercept_cert():
            return False
            
        try:
            self.proxy_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.proxy_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.proxy_socket.bind(('localhost', listen_port))
            self.proxy_socket.listen(5)
            self.running = True
            
            print(f"MITM Proxy listening on localhost:{listen_port}")
            print(f"Intercepting traffic to {self.target_host}:{self.target_port}")
            
            return True
        except Exception as e:
            print(f"Failed to start proxy: {e}")
            return False
    
    def handle_client(self, client_socket, client_addr):
        try:
            server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server_context = ssl.create_default_context()
            server_context.check_hostname = False
            server_context.verify_mode = ssl.CERT_NONE
            
            server_ssl = server_context.wrap_socket(
                server_socket, 
                server_hostname=self.target_host
            )
            server_ssl.connect((self.target_host, self.target_port))
            print(f"Intercepted connection from {client_addr}")
            
            def relay(src, dst, name):
                try:
                    while self.running:
                        data = src.recv(4096)
                        if not data:
                            break
                        print(f"[{name}] {len(data)} bytes")
                        dst.send(data)
                except Exception as e:
                    print(f"Relay error: {e}")
                finally:
                    try:
                        src.close()
                        dst.close()
                    except:
                        pass

            client_to_server = threading.Thread(
                target=relay, 
                args=(client_socket, server_ssl, "Client->Server")
            )
            server_to_client = threading.Thread(
                target=relay, 
                args=(server_ssl, client_socket, "Server->Client")
            )
            
            client_to_server.daemon = True
            server_to_client.daemon = True
            client_to_server.start()
            server_to_client.start()
            client_to_server.join()
            server_to_client.join()
            
        except Exception as e:
            print(f"Error handling client {client_addr}: {e}")
        finally:
            try:
                client_socket.close()
            except:
                pass
    
    def run(self, listen_port):
        """Run the proxy server"""
        if not self.start_proxy(listen_port):
            return False
            
        try:
            while self.running:
                try:
                    client_socket, client_addr = self.proxy_socket.accept()
                    client_thread = threading.Thread(
                        target=self.handle_client,
                        args=(client_socket, client_addr)
                    )
                    client_thread.daemon = True
                    client_thread.start()
                    
                except Exception as e:
                    if self.running:
                        print(f"Error accepting connection: {e}")
                        
        except KeyboardInterrupt:
            print("Shutting down proxy...")
        finally:
            self.running = False
            if self.proxy_socket:
                self.proxy_socket.close()
        
        return True

def main():
    if len(sys.argv) != 4:
        print("Usage: python3 mitm_proxy.py <target_host> <target_port> <listen_port>")
        print("Example: python3 mitm_proxy.py test.mosquitto.org 8883 8884")
        sys.exit(1)
    
    target_host = sys.argv[1]
    target_port = int(sys.argv[2])
    listen_port = int(sys.argv[3])
    
    proxy = MITMProxy(target_host, target_port)
    
    print(f"Starting MITM proxy test...")
    print(f"Target: {target_host}:{target_port}")
    print(f"Listening on: localhost:{listen_port}")
    
    try:
        proxy.run(listen_port)
    except KeyboardInterrupt:
        print("\nProxy stopped by user")

if __name__ == "__main__":
    main()