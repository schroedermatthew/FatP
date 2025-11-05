#!/bin/bash
# Build script for Tensor MKL integration

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}  Tensor MKL Build Script${NC}"
echo -e "${BLUE}============================================${NC}"
echo ""

# Configuration
BUILD_DIR="build"
USE_MKL=${USE_MKL:-1}
BUILD_TYPE=${BUILD_TYPE:-Release}
NUM_THREADS=${NUM_THREADS:-$(nproc)}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-mkl)
            USE_MKL=0
            shift
            ;;
        --debug)
            BUILD_TYPE=Debug
            shift
            ;;
        --clean)
            echo -e "${YELLOW}Cleaning build directory...${NC}"
            rm -rf "$BUILD_DIR"
            shift
            ;;
        -j*)
            NUM_THREADS="${1#-j}"
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --no-mkl      Build without MKL (use default backend)"
            echo "  --debug       Build in Debug mode (default: Release)"
            echo "  --clean       Clean build directory before building"
            echo "  -jN           Use N parallel jobs (default: nproc)"
            echo "  --help        Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check for MKL
if [ "$USE_MKL" -eq 1 ]; then
    echo -e "${YELLOW}Checking for Intel MKL...${NC}"
    
    # Try to source MKL environment
    if [ -f "/opt/intel/oneapi/setvars.sh" ]; then
        echo -e "${GREEN}Found Intel oneAPI, sourcing environment...${NC}"
        source /opt/intel/oneapi/setvars.sh --force > /dev/null 2>&1
    elif [ -f "$HOME/intel/oneapi/setvars.sh" ]; then
        echo -e "${GREEN}Found Intel oneAPI in home directory...${NC}"
        source "$HOME/intel/oneapi/setvars.sh" --force > /dev/null 2>&1
    else
        echo -e "${YELLOW}Warning: MKL environment not found, may need manual configuration${NC}"
    fi
fi

# Create build directory
echo -e "${BLUE}Creating build directory...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${BLUE}Configuring project...${NC}"
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DUSE_MKL="$USE_MKL"
    -DBUILD_TESTS=ON
    -DBUILD_EXAMPLES=ON
)

if ! cmake .. "${CMAKE_ARGS[@]}"; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

# Build
echo -e "${BLUE}Building project (using $NUM_THREADS threads)...${NC}"
if ! cmake --build . -j"$NUM_THREADS"; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Build Successful!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""

# Show built executables
if [ -f "test_tensor" ]; then
    echo -e "${GREEN}✓ test_tensor${NC}"
fi
if [ -f "test_tensor_mkl" ]; then
    echo -e "${GREEN}✓ test_tensor_mkl${NC}"
fi
if [ -f "example_tensor_mkl" ]; then
    echo -e "${GREEN}✓ example_tensor_mkl${NC}"
fi

echo ""
echo "Run tests with:"
echo -e "  ${BLUE}cd $BUILD_DIR && ctest${NC}"
echo ""
echo "Or run individual executables:"
if [ -f "test_tensor" ]; then
    echo -e "  ${BLUE}./$BUILD_DIR/test_tensor${NC}"
fi
if [ -f "test_tensor_mkl" ]; then
    echo -e "  ${BLUE}./$BUILD_DIR/test_tensor_mkl${NC}"
fi
if [ -f "example_tensor_mkl" ]; then
    echo -e "  ${BLUE}./$BUILD_DIR/example_tensor_mkl${NC}"
fi
echo ""
