#!/bin/bash
# MITM Test Runner Script

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
SIMULATOR_PATH="$PROJECT_ROOT/build/iot_gateway_sim"

echo "IoT Gateway Simulator MITM Security Testing"
echo "=========================================="

# Check if simulator exists
if [ ! -f "$SIMULATOR_PATH" ]; then
    echo "ERROR: IoT Gateway Simulator not found at $SIMULATOR_PATH"
    echo "Please build the project first with 'make' or 'cmake'"
    exit 1
fi

# Run the MITM tests
echo "Running MITM security tests..."
python3 "$SCRIPT_DIR/mitm_test.py"

# Capture exit code
EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo "All tests completed successfully!"
else
    echo "Some tests failed. Check output above for details."
fi

exit $EXIT_CODE