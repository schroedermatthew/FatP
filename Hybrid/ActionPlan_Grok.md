# Action Plan for Step 1: Environment Setup and Dependency Installation

**Objective**: Set up a fully functional Python development environment for the strip decomposition baseline. Install all required libraries, clone and evaluate reusable GitHub repositories for boustrophedon/strip generation, and verify basic geometry operations with Shapely. Achieve a "hello-world" test: Generate a simple strip decomposition on a square polygon.

**Timeline**: Days 1-2 (4-8 hours total, assuming prior Python experience).

**Assumptions**:
- Working on Linux/macOS/Windows with Python 3.10+ pre-installed.
- Git installed.
- GPU optional (not needed yet).
- Project directory: Create a new folder `neural-geometry-sar/` and work inside it.

## 1. Create Project Structure and Virtual Environment (30-45 minutes)

- Create project folder:
  ```
  mkdir neural-geometry-sar
  cd neural-geometry-sar
  ```

- Initialize Git repo (for version control and easy forking later):
  ```
  git init
  ```

- Create virtual environment:
  ```
  python -m venv venv
  source venv/bin/activate  # On Windows: venv\Scripts\activate
  ```

- Upgrade pip and install base tools:
  ```
  pip install --upgrade pip setuptools wheel
  ```

- Create `requirements.txt` file with core dependencies:
  ```
  numpy
  scipy
  matplotlib
  shapely>=2.0
  scikit-learn
  torch
  torchvision
  fiona  # For potential shapefile I/O later
  geopandas  # Optional but useful for visualization
  ```

- Install them:
  ```
  pip install -r requirements.txt
  ```

- Add a `.gitignore` (standard Python one):
  - Download from https://github.com/github/gitignore/blob/main/Python.gitignore or create manually (ignore venv, __pycache__, etc.).

## 2. Install and Test Core Geometry Library: Shapely (45-60 minutes)

- Verify installation:
  ```python
  python -c "import shapely; print(shapely.__version__)"
  ```
  Expected: 2.0+.

- Create `test_shapely.py` for basic polygon operations:
  ```python
  from shapely.geometry import Polygon, LineString
  from shapely.ops import split

  # Simple square polygon
  square = Polygon([(0,0), (100,0), (100,100), (0,100), (0,0)])
  print("Area:", square.area)

  # Test clipping with a line
  line = LineString([(50,0), (50,100)])
  clipped = split(square, line)
  print("Clipped parts:", len(list(clipped.geoms)))
  ```

- Run it:
  ```
  python test_shapely.py
  ```
  Expected: Area 10000, 2 clipped parts.

- Test projection for W_total(φ) calculation (key for strips):
  ```python
  import numpy as np
  from shapely.geometry import Polygon

  poly = Polygon([(0,0), (100,0), (100,100), (0,100)])
  coords = np.array(poly.exterior.coords)

  phi = np.pi / 4  # 45 degrees
  n_phi = np.array([-np.sin(phi), np.cos(phi)])  # Perpendicular normal

  projections = coords @ n_phi
  w_total = projections.max() - projections.min()
  print("W_total at 45°:", w_total)  # Should be ~141.42
  ```

- This confirms Shapely handles basic polygon projections needed for strip extent.

## 3. Clone and Evaluate Reusable Coverage Planning Repositories (2-3 hours)

### Primary Target: trajgenpy (Pure Python, Boustrophedon Coverage)
- Install directly (it's on PyPI and matches coverage trajectory generation):
  ```
  pip install trajgenpy
  ```

- Clone source for inspection/modification:
  ```
  git clone https://github.com/kasperg3/trajgenpy.git
  cd trajgenpy
  pip install -e .  # Editable install if needed
  ```

- Test basic usage (from repo README/examples):
  ```python
  import trajgenpy
  from shapely.geometry import Polygon

  # Example polygon
  poly = Polygon([(0,0), (100,0), (100,50), (0,50)])

  # Generate coverage trajectory (boustrophedon)
  trajectory = trajgenpy.generate_coverage_path(poly, sweep_angle=0, sweep_width=10)
  print(trajectory)  # Should output waypoints or strips
  ```

- If it generates parallel strips: Success. Modify to output strip Polygons instead of paths.

### Fallback 1: Fields2Cover (Advanced, C++ with Python bindings)
- Compilation required (heavy dependencies: GDAL, SWIG, etc.).
- Only pursue if trajgenpy lacks features.
- Commands (from docs):
  ```
  sudo apt-get install libgdal-dev swig cmake g++  # Ubuntu example
  git clone https://github.com/Fields2Cover/Fields2Cover.git
  cd Fields2Cover
  mkdir build && cd build
  cmake -DBUILD_PYTHON=ON ..
  make -j$(nproc)
  sudo make install
  ```

- Test Python import:
  ```python
  python -c "import fields2cover as f2c; print(f2c.__version__)"
  ```

### Fallback 2: Greenzie/boustrophedon_planner (ROS/C++, archived)
- Archived (2024), ROS-dependent → Not ideal for pure Python.
- Skip unless needing ROS integration later.
- Clone for reference only:
  ```
  git clone https://github.com/Greenzie/boustrophedon_planner.git
  ```

### Other Pure Python References
- Search yielded RicheyHuang/BoustrophedonCellularDecompositionPathPlanning (simple toy code) → Clone if needed:
  ```
  git clone https://github.com/RicheyHuang/BoustrophedonCellularDecompositionPathPlanning.git
  ```

## 4. Hello-World Test: Manual Strip Decomposition with Shapely (1 hour)

- If no repo gives instant strips, implement minimal version:
  Create `strip_decomposer.py`:
  ```python
  import numpy as np
  from shapely.geometry import Polygon, LineString
  from shapely.ops import split

  def decompose_to_strips(poly: Polygon, phi: float, num_strips: int = 8):
      coords = np.array(poly.exterior.coords[:-1])  # Drop closing point
      n_phi = np.array([-np.sin(phi), np.cos(phi)])
      projections = coords @ n_phi
      min_proj, max_proj = projections.min(), projections.max()
      w_total = max_proj - min_proj
      strip_width = w_total / num_strips

      strips = []
      current_offset = min_proj
      for i in range(num_strips):
          line1 = LineString([ (x - 1000*n_phi[0], y - 1000*n_phi[1]) for x,y in coords ])  # Infinite line approximation
          # Better: Use parallel lines at offsets
          # Simplified: Split sequentially
          current_offset += strip_width
          # Use buffer or proper clipping for production
          strips.append(poly)  # Placeholder
      return strips  # Refine with actual clipping examples from searches

  # Test
  square = Polygon([(0,0),(100,0),(100,100),(0,100)])
  strips = decompose_to_strips(square, phi=np.pi/4, num_strips=8)
  print(len(strips))
  ```

- Refine using clipping examples from searches (e.g., split with parallel lines).

## 5. Verification and Documentation (30 minutes)

- Run all tests → No errors.
- Commit initial setup:
  ```
  git add .
  git commit -m "Initial environment setup and Shapely tests"
  ```

- Document in README.md: List installed packages, tested repos, any issues.

**Milestone Achievement**: Environment ready, basic strip generation working (via trajgenpy or custom Shapely). Proceed to Step 2 (data generation) only after this succeeds.

**Troubleshooting**:
- Shapely errors: Ensure GEOS installed (brew install geos on macOS).
- Import failures: Check Python version compatibility.
- If trajgenpy insufficient: Fall back to custom Shapely implementation using projection + parallel line clipping.