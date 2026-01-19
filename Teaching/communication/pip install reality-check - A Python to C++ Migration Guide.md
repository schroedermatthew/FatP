# pip install reality-check
## A Python → C++ Migration Guide for the Dynamically Typed

*"It works in Jupyter."*

---

## Know Your Adversary

You've been asked to productionize a machine learning pipeline. It runs in a Jupyter notebook. It's written in Python. It "works"—meaning it produces correct output when run manually by its creator on a good day with the right version of every dependency.

The creator—let's call her Dr. Notebook—has mass: a PhD in a quantitative field, mass: zero software engineering training. She learned Python because that's what the tutorials used. She doesn't write code; she *conducts experiments*. The notebook is not a program—it's a *record of exploration*.

Dr. Notebook exhibits the following clinical symptoms:

**Execution Order Amnesia.** The notebook runs correctly when cells are executed in order. But the cells have been executed out of order hundreds of times during development. Some cells depend on variables defined in cells that no longer exist. Some cells redefine variables that other cells expect to remain constant. The "correct" execution order is stored in Dr. Notebook's head, reconstructed from memory each time she presents results.

```python
# Cell 47 (must be run after cell 12, but before cell 23, unless you ran cell 31)
df = df.dropna()  # Which df? The one from cell 8 or the one from cell 34?
model.fit(X, y)   # Defined where? Who knows. Run all cells and pray.
```

**Import Spaghetti.** The notebook begins with 47 import statements, accumulated over months of development. Half are unused. Some are aliased inconsistently (`import pandas as pd` in cell 1, `import pandas` in cell 89). Some are conditional imports inside try/except blocks because the notebook needs to run on both her laptop and the lab's GPU server.

```python
import numpy as np
import pandas as pd  
import pandas  # Also this, for some reason
from sklearn.preprocessing import *  # Why specify?
import matplotlib.pyplot as plt
%matplotlib inline
from tqdm.notebook import tqdm  # Or is it tqdm.auto? Depends on the day.
try:
    import torch
except:
    import tensorflow as tf  # Fallback? Alternative? Who knows.
```

**Global Variable Proliferation.** Everything is global. The data is global. The model is global. The hyperparameters are global. Intermediate results are global. This isn't laziness—it's *convenience*. Dr. Notebook can inspect any variable at any time by typing its name in a cell. Encapsulation would make debugging harder.

```python
# Cell 1
data = load_data()

# Cell 47
# data is still here, right? We didn't accidentally overwrite it in cell 23?
# Let's just check... *runs cell 1 again to be safe*
```

**Type Fluidity.** Variables change type freely. `x` might be a list, then a numpy array, then a pandas Series, then back to a list. Dr. Notebook doesn't think about types—she thinks about *shapes*. "x is (1000, 50)" is more meaningful to her than "x is np.ndarray". When shapes match, code works. When shapes don't match, she gets an error message and fixes it. This is her type system.

```python
x = [1, 2, 3]              # List
x = np.array(x)            # Now ndarray  
x = pd.Series(x)           # Now Series
x = x.values               # Back to ndarray
x = x.tolist()             # List again
# All in the same logical "variable"
```

**The Magic Incantation Pattern.** Dr. Notebook doesn't always understand why code works. Some lines were copied from Stack Overflow. Some were suggested by ChatGPT. Some are cargo-culted from tutorials. They work. Don't touch them.

```python
# Don't remove this line. I don't know why it's needed but the model
# doesn't converge without it.
np.random.seed(42)
torch.manual_seed(42)
torch.cuda.manual_seed_all(42)
torch.backends.cudnn.deterministic = True
torch.backends.cudnn.benchmark = False
os.environ['PYTHONHASHSEED'] = str(42)
```

**Output-Oriented Development.** The code is correct if the output looks correct. There are no tests—why would there be? Dr. Notebook *looks* at the output. She runs the cell, sees the plot, checks the numbers. If they look right, they are right. The idea of automated testing is foreign; testing is what humans do by visual inspection.

**Dependency Denial.** The notebook requires 147 packages. They're not listed anywhere. When someone else tries to run the notebook, they get ImportError, fix it with `pip install`, and repeat until it works. The specific versions matter, but nobody knows which ones. "It works on my machine" is both explanation and defense.

---

## The Migration Battlefield

### Battle #1: The Compilation Confrontation

**The Conflict**

Python doesn't compile. You run it. C++ compiles, then runs. This two-step process seems like bureaucratic overhead to Dr. Notebook.

"Why can't I just run it? Why do I have to compile first? This is slower."

She will attempt to write C++ like Python:

```cpp
// Dr. Notebook's first C++ attempt
#include <iostream>

int main() {
    x = 5;  // ERROR: 'x' was not declared
    std::cout << x << std::endl;
}
```

"Why doesn't it know x is an integer? It's obviously an integer. I assigned 5 to it."

**Arguments That Don't Work**

- "C++ is statically typed." (What does that mean?)
- "You need to declare variables." (That's redundant.)
- "The compiler catches errors." (So does Python. I run it and see errors.)

**Arguments That Do Work**

- "Remember when you spent 6 hours debugging that shape mismatch that only appeared on the full dataset? C++ would have caught that before running any code. The compiler finds bugs at write time, not at run time." (Past pain is memorable.)
- "Compilation is one-time. Once compiled, the code runs 100x faster every time. The notebook re-interprets every line every time. You're paying the 'compilation' cost on every run." (Reframe the tradeoff.)
- "The compile step takes 3 seconds. The training run takes 8 hours. Would you rather find bugs in 3 seconds or 4 hours into training?" (Concrete time savings.)

**The Correct Migration**

```cpp
// C++: explicit declarations catch errors early
#include <iostream>
#include <vector>
#include <numeric>

int main() {
    int x = 5;  // Type is explicit
    
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    // Compiler knows data contains doubles
    // Compiler knows std::accumulate returns double
    // Compiler catches type mismatches immediately
    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    
    std::cout << "Sum: " << sum << std::endl;
    return 0;
}
```

The key insight for Dr. Notebook: **Compilation is instant feedback. Python gives you feedback when the bug executes. C++ gives you feedback when you write the bug.**

---

### Battle #2: The Type System Tussle

**The Conflict**

Python variables can hold any type. Dr. Notebook relies on this. She doesn't think about types—she thinks about data.

```python
# Python: types are fluid
result = process(data)  # result could be anything
# Just print it and see what you got
print(type(result), result.shape if hasattr(result, 'shape') else len(result))
```

When you explain C++ requires type declarations, she'll ask: "Why can't it just figure out the type? Python figures it out."

She might try:

```cpp
// Dr. Notebook's attempt at Python-style dynamism
#include <any>

int main() {
    std::any result = process(data);
    // Now what? How do I use result?
    // std::cout << result.shape; // ERROR: 'any' has no member 'shape'
}
```

**Arguments That Don't Work**

- "Static typing is safer." (I don't have type bugs. I check the output.)
- "The compiler needs to know the type." (That's the compiler's problem, not mine.)
- "Types are documentation." (I have docstrings. Sometimes.)

**Arguments That Do Work**

- "You spend 30% of your debugging time on shape mismatches. In C++, the type *includes* the shape at compile time. `Eigen::Matrix<double, 100, 50>` is a 100×50 matrix. Assigning a 50×100 matrix is a compile error, not a runtime error in hour 7 of training." (Solve her actual problem.)
- "C++ has `auto` for type inference. You write `auto result = process(data);` and the compiler figures out the type. Same convenience, but the type is checked at compile time." (Show the escape hatch.)
- "NumPy is written in C++. The 'duck typing' you love in Python is calling into static types underneath. We're just removing the Python layer." (Peek behind the curtain.)

**The Correct Migration**

```cpp
// C++: static types with inference

// Explicit when it matters
Eigen::MatrixXd data(1000, 50);  // 1000 samples, 50 features

// auto for convenience when types are obvious
auto normalized = (data.rowwise() - data.colwise().mean());
auto covariance = (normalized.transpose() * normalized) / (data.rows() - 1);

// Templates for generic code
template<typename MatrixType>
auto compute_pca(const MatrixType& data, int n_components) {
    // Works for any matrix type - compile-time polymorphism
    Eigen::JacobiSVD<MatrixType> svd(data, Eigen::ComputeThinU | Eigen::ComputeThinV);
    return svd.matrixV().leftCols(n_components);
}
```

The key insight: **auto gives you Python-like type inference with C++ type safety. The compiler knows the type; you don't have to write it; but mistakes are still caught.**

---

### Battle #3: The Memory Management Meltdown

**The Conflict**

Python has garbage collection. Dr. Notebook creates objects freely, and they disappear when no longer needed. Memory is infinite and free.

```python
# Python: memory appears and vanishes magically
for i in range(1000):
    huge_matrix = np.random.randn(10000, 10000)  # 800MB each
    result = process(huge_matrix)
    # huge_matrix just... goes away somehow
```

In C++, memory is explicit. Dr. Notebook will try:

```cpp
// Dr. Notebook's attempt
for (int i = 0; i < 1000; ++i) {
    double* huge_matrix = new double[100000000];  // 800MB
    process(huge_matrix);
    // Where does it go? Nowhere. Memory leak.
}
// Program now uses 800GB of RAM. System crashes.
```

When you mention `delete`, she'll ask: "Why do I have to manage memory? Python doesn't make me do this."

**Arguments That Don't Work**

- "C++ requires manual memory management." (Sounds terrible. Why would I want that?)
- "You need to delete what you new." (So I have to track every allocation? That's error-prone.)
- "Memory leaks are bad." (I'll just restart the kernel.)

**Arguments That Do Work**

- "Remember when your training crashed at epoch 47 because Python's garbage collector paused for 30 seconds and your GPU timed out? C++ doesn't have garbage collection pauses. Memory is freed deterministically, exactly when you expect." (Solve a real problem.)
- "C++ has smart pointers that work like Python's memory management. `std::vector` allocates and frees automatically. You never write `new` or `delete` in modern C++." (Show that RAII is automatic.)
- "The reason numpy is fast is because it manages memory explicitly. We're not adding work—we're making the hidden work visible and controllable." (Reveal the hidden complexity.)

**The Correct Migration**

```cpp
// Modern C++: RAII means automatic memory management

#include <vector>
#include <Eigen/Dense>

void train() {
    for (int i = 0; i < 1000; ++i) {
        // Eigen matrix manages its own memory
        Eigen::MatrixXd huge_matrix = Eigen::MatrixXd::Random(10000, 10000);
        
        process(huge_matrix);
        
        // huge_matrix automatically freed here - no leak, no GC pause
    }
}

// For data that outlives the scope
std::unique_ptr<Model> train_model(const Data& data) {
    auto model = std::make_unique<Model>();
    model->fit(data);
    return model;  // Ownership transferred to caller
}
```

The key insight: **Modern C++ uses RAII. Memory is freed when scope ends—like Python, but deterministic. No garbage collector. No pauses. No leaks if you follow the idiom.**

---

### Battle #4: The Package Manager Panic

**The Conflict**

In Python, installing packages is `pip install whatever`. Need numpy? `pip install numpy`. Need a obscure computer vision library from 2019? `pip install opencv-contrib-python-headless==4.5.3.56`.

C++ has no `pip`. Dr. Notebook will ask: "How do I install Eigen? How do I install a JSON library? Where's the package manager?"

```bash
# Python: easy
pip install numpy pandas scikit-learn torch transformers

# C++: ???
```

**Arguments That Don't Work**

- "C++ has CMake." (Is that a package manager?)
- "You can use vcpkg or Conan." (Why are there multiple? Which one?)
- "Header-only libraries just need an include." (Where do I get the header?)

**Arguments That Do Work**

- "It's harder to install libraries in C++, but you only do it once per project. The payoff is reproducibility—you'll never have the 'works on my machine' problem again because dependencies are explicit and versioned." (Trade setup time for runtime reliability.)
- "Let me configure the build system. You'll just clone the repo and type `cmake --build`. The complexity is frontloaded but then it's done." (Offer to do the hard part.)
- "Your notebook has 147 implicit dependencies. I found 3 version conflicts that produce different results on different machines. The C++ build makes every dependency explicit and version-locked." (Show the hidden mess.)

**The Practical Setup**

```cmake
# CMakeLists.txt - you write this once
cmake_minimum_required(VERSION 3.16)
project(ml_pipeline CXX)

set(CMAKE_CXX_STANDARD 20)

# Dependencies managed explicitly
find_package(Eigen3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

add_executable(train
    src/main.cpp
    src/model.cpp
    src/data_loader.cpp
)

target_link_libraries(train PRIVATE 
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    fmt::fmt
)
```

```bash
# User experience after setup
git clone <repo>
cd repo
cmake -B build
cmake --build build
./build/train  # Just works. Every time. On every machine.
```

---

### Battle #5: The Notebook Nostalgia

**The Conflict**

Dr. Notebook loves Jupyter because she can see intermediate results. Run a cell, see output. Tweak parameters, re-run. This interactive workflow is central to her process.

"How do I do exploratory analysis in C++? How do I see intermediate results? How do I iterate quickly?"

She might try to recreate notebooks:

```cpp
// Dr. Notebook's desperation
int main() {
    auto data = load_data();
    std::cout << "data shape: " << data.rows() << "x" << data.cols() << "\n";
    std::cout << "data sample:\n" << data.topRows(5) << "\n";
    // Compile. Run. Look at output. Change code. Recompile. Repeat.
    // This is terrible.
}
```

**Arguments That Don't Work**

- "Use a debugger." (Debuggers are for finding bugs, not exploring data.)
- "Print statements work." (I have to recompile for every print!)
- "C++ isn't for exploration." (Then why are we using it?)

**Arguments That Do Work**

- "Keep Python for exploration. Use C++ for production. They're different tools for different jobs. Your notebook is a scratchpad; the C++ code is the final product." (Separate concerns.)
- "Exploration is 5% of the time. Training and inference are 95%. Optimize for the 95%." (Time allocation argument.)
- "There are C++ notebooks (xeus-cling). They're not as polished, but they exist for when you need interactive C++." (Partial solution.)

**The Realistic Workflow**

```
┌─────────────────────────────────────────────────────────────┐
│                    EXPLORATION PHASE                        │
│                    (Keep using Python)                      │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │   Jupyter   │ →  │  Prototype  │ →  │   Validate  │     │
│  │   Notebook  │    │  Algorithm  │    │   Results   │     │
│  └─────────────┘    └─────────────┘    └─────────────┘     │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼ Algorithm finalized
┌─────────────────────────────────────────────────────────────┐
│                   PRODUCTION PHASE                          │
│                   (Migrate to C++)                          │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │  Translate  │ →  │    Test     │ →  │   Deploy    │     │
│  │  to C++     │    │  (Automated)│    │  (100x fast)│     │
│  └─────────────┘    └─────────────┘    └─────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

The key insight: **Python for exploration, C++ for production. The notebook is your lab; C++ is the factory.**

---

### Battle #6: The List Comprehension Lament

**The Conflict**

Dr. Notebook writes beautiful one-liners:

```python
# Python: elegant
result = [x**2 for x in data if x > 0]
filtered = {k: v for k, v in items.items() if v is not None}
flattened = [item for sublist in nested for item in sublist]
```

C++ has no list comprehensions. Dr. Notebook will mourn:

```cpp
// C++: verbose
std::vector<double> result;
for (double x : data) {
    if (x > 0) {
        result.push_back(x * x);
    }
}
// Four lines instead of one! Barbaric!
```

**Arguments That Don't Work**

- "Explicit loops are clearer." (No they're not. The list comprehension is crystal clear.)
- "C++ doesn't have comprehensions." (Why not? It's 2024.)
- "You'll get used to it." (I don't want to get used to worse.)

**Arguments That Do Work**

- "C++20 ranges are similar to comprehensions. `data | filter(positive) | transform(square)` reads like Python and compiles to optimal code." (Show the modern syntax.)
- "The explicit loop runs 50x faster than the list comprehension because there's no interpreter overhead and the compiler can vectorize it." (Performance payoff.)
- "Python's comprehension creates a temporary list. C++ ranges are lazy—they don't allocate intermediate storage. It's more memory-efficient than Python even though it looks longer." (Hidden cost of Python convenience.)

**The Correct Migration**

```cpp
// C++20 ranges: functional style, zero overhead

#include <ranges>
#include <vector>
#include <algorithm>

// Python: result = [x**2 for x in data if x > 0]
auto positive = [](double x) { return x > 0; };
auto square = [](double x) { return x * x; };

auto result = data 
    | std::views::filter(positive)
    | std::views::transform(square);

// To materialize into a vector:
std::vector<double> vec(result.begin(), result.end());

// Or use ranges algorithms directly:
std::vector<double> vec;
std::ranges::copy(
    data | std::views::filter(positive) | std::views::transform(square),
    std::back_inserter(vec)
);
```

For pre-C++20 or when performance is critical:

```cpp
// Pre-allocated loop: fastest possible
std::vector<double> result;
result.reserve(data.size());  // Avoid reallocations

for (double x : data) {
    if (x > 0) {
        result.push_back(x * x);
    }
}
```

---

### Battle #7: The DataFrame Dependency

**The Conflict**

Dr. Notebook's entire worldview is built on pandas DataFrames:

```python
# Python: DataFrame is life
df = pd.read_csv('data.csv')
df = df.dropna()
df['new_col'] = df['a'] + df['b']
grouped = df.groupby('category').mean()
result = df.merge(other_df, on='key')
```

C++ has no pandas. This is a crisis.

"How do I load a CSV? How do I filter columns? How do I do groupby? There's no pandas!"

**Arguments That Don't Work**

- "Write your own data structures." (I don't want to reinvent pandas.)
- "Use a database." (I want DataFrames, not SQL.)
- "Parse the CSV manually." (That's insane.)

**Arguments That Do Work**

- "For ML pipelines, you usually convert the DataFrame to numpy arrays anyway. In C++, we go directly to the matrix representation. The DataFrame is scaffolding; we're removing scaffolding." (Challenge the need.)
- "There are C++ DataFrame libraries (DataFrame, xtensor, Polars has a C++ backend). They're not identical to pandas, but they handle tabular data." (Solutions exist.)
- "Your DataFrame operations take 40% of pipeline runtime. Direct matrix operations are 10x faster. The 'convenience' of pandas costs you hours of training time." (Performance cost of abstraction.)

**The Correct Migration**

```cpp
// Option 1: Direct to matrices (most ML pipelines)

#include <Eigen/Dense>
#include <rapidcsv.h>

Eigen::MatrixXd load_features(const std::string& path, 
                               const std::vector<std::string>& columns) {
    rapidcsv::Document doc(path);
    
    size_t n_rows = doc.GetRowCount();
    size_t n_cols = columns.size();
    
    Eigen::MatrixXd data(n_rows, n_cols);
    
    for (size_t j = 0; j < n_cols; ++j) {
        auto col = doc.GetColumn<double>(columns[j]);
        for (size_t i = 0; i < n_rows; ++i) {
            data(i, j) = col[i];
        }
    }
    
    return data;
}

// Option 2: Use a DataFrame library for exploration
#include <DataFrame/DataFrame.h>

using namespace hmdf;

StdDataFrame<unsigned long> df;
df.read("data.csv");

auto filtered = df.get_view_by_sel<double, int>(
    "column_name",
    [](const double& val) { return val > 0; }
);
```

---

### Battle #8: The Machine Learning Library Maze

**The Conflict**

In Python, ML is `from sklearn import *`. Neural networks are `import torch` or `import tensorflow`. Pre-trained models are `from transformers import pipeline`.

"Where's sklearn for C++? Where's PyTorch? How do I load a pre-trained BERT model?"

**Arguments That Don't Work**

- "Implement the algorithms yourself." (I'm not implementing gradient descent by hand.)
- "C++ is for low-level code." (So I can't use it for ML?)
- "Python is better for ML." (Then why are we doing this migration?)

**Arguments That Do Work**

- "PyTorch has a C++ frontend (LibTorch) with the same API as Python. Your trained model ports directly." (Direct solution.)
- "For inference, C++ is the standard. TensorFlow, PyTorch, ONNX all have C++ inference engines. Your model trains in Python and serves in C++." (Industry standard pattern.)
- "For classical ML (random forests, SVMs), there's mlpack, Shogun, and Shark. They're faster than sklearn." (Libraries exist.)

**The Practical Approach**

```cpp
// PyTorch model inference in C++ (LibTorch)

#include <torch/script.h>

class ModelInference {
public:
    explicit ModelInference(const std::string& model_path) {
        model_ = torch::jit::load(model_path);
        model_.eval();
    }
    
    torch::Tensor predict(const torch::Tensor& input) {
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(input);
        return model_.forward(inputs).toTensor();
    }
    
private:
    torch::jit::script::Module model_;
};

// Usage
int main() {
    ModelInference model("trained_model.pt");
    
    auto input = torch::randn({1, 3, 224, 224});
    auto output = model.predict(input);
    
    std::cout << "Prediction: " << output.argmax(1).item<int>() << "\n";
}
```

The key insight: **Train in Python, serve in C++. This is the industry standard because it combines Python's ecosystem with C++ performance.**

---

## Technical Reference: Python → C++ Patterns

### Type Mappings

| Python | C++ | Notes |
|--------|-----|-------|
| `int` | `int64_t` | Python ints are arbitrary precision; pick fixed size |
| `float` | `double` | Python float is C double |
| `bool` | `bool` | Same |
| `str` | `std::string` | |
| `list` | `std::vector<T>` | Homogeneous in C++ |
| `dict` | `std::unordered_map<K,V>` | Homogeneous in C++ |
| `set` | `std::unordered_set<T>` | |
| `tuple` | `std::tuple<T...>` | |
| `None` | `std::nullopt` / `nullptr` | With `std::optional<T>` |
| `np.ndarray` | `Eigen::MatrixXd` | Or `std::vector<double>` |
| `pd.DataFrame` | Custom struct / matrix | No direct equivalent |

### NumPy → Eigen

```cpp
// np.zeros((m, n))
Eigen::MatrixXd::Zero(m, n);

// np.ones((m, n))
Eigen::MatrixXd::Ones(m, n);

// np.eye(n)
Eigen::MatrixXd::Identity(n, n);

// np.random.randn(m, n)
Eigen::MatrixXd::Random(m, n);  // Uniform [-1, 1], not normal!

// For normal distribution:
#include <random>
std::mt19937 gen(42);
std::normal_distribution<> d(0, 1);
Eigen::MatrixXd mat(m, n);
for (int i = 0; i < m; ++i)
    for (int j = 0; j < n; ++j)
        mat(i, j) = d(gen);

// np.dot(A, B) or A @ B
A * B  // Matrix multiplication

// A * B (element-wise)
A.cwiseProduct(B)  // Or A.array() * B.array()

// A.T
A.transpose()

// A.sum()
A.sum()

// A.mean()
A.mean()

// A.sum(axis=0)  (column sums)
A.colwise().sum()

// A.sum(axis=1)  (row sums)
A.rowwise().sum()

// A[A > 0]  (boolean indexing)
// No direct equivalent; use loop or select
Eigen::VectorXd positive = (A.array() > 0).select(A, 0);

// np.concatenate([A, B], axis=0)
Eigen::MatrixXd C(A.rows() + B.rows(), A.cols());
C << A, B;

// np.concatenate([A, B], axis=1)
Eigen::MatrixXd C(A.rows(), A.cols() + B.cols());
C << A, B;

// np.linalg.inv(A)
A.inverse()

// np.linalg.solve(A, b)
A.lu().solve(b)  // Or other decompositions

// np.linalg.svd(A)
Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
svd.singularValues();
svd.matrixU();
svd.matrixV();
```

### Control Flow

```cpp
// Python: for x in items
for (const auto& x : items)

// Python: for i, x in enumerate(items)
for (size_t i = 0; i < items.size(); ++i) {
    const auto& x = items[i];
}

// Python: for k, v in dict.items()
for (const auto& [k, v] : map)

// Python: if x in container
if (container.find(x) != container.end())
if (container.count(x) > 0)
if (container.contains(x))  // C++20

// Python: try/except
try {
    // ...
} catch (const std::exception& e) {
    // ...
}

// Python: with open(path) as f
{
    std::ifstream f(path);
    // file closed when scope ends
}

// Python: lambda x: x + 1
auto lambda = [](int x) { return x + 1; };

// Python: map(f, items)
std::transform(items.begin(), items.end(), output.begin(), f);

// Python: filter(pred, items)
std::copy_if(items.begin(), items.end(), std::back_inserter(output), pred);

// Python: any(pred(x) for x in items)
std::any_of(items.begin(), items.end(), pred)

// Python: all(pred(x) for x in items)
std::all_of(items.begin(), items.end(), pred)
```

### Common Gotchas

```cpp
// GOTCHA: Integer division
// Python: 5 / 2 = 2.5 (always float)
// C++: 5 / 2 = 2 (integer truncation)
double result = 5.0 / 2;  // Or static_cast<double>(5) / 2

// GOTCHA: Negative modulo
// Python: -7 % 3 = 2 (always positive)
// C++: -7 % 3 = -1 (sign of dividend)
int python_mod = ((a % b) + b) % b;  // For Python semantics

// GOTCHA: Boolean conversion
// Python: if [1, 2, 3]:  (truthy if non-empty)
// C++: if (vec) won't compile
if (!vec.empty())  // Explicit check

// GOTCHA: String indexing
// Python: s[-1]  (last character)
// C++: s[-1] undefined behavior
s.back()  // Or s[s.size() - 1]

// GOTCHA: Default arguments
// Python: def f(x=[]):  (mutable default, shared across calls)
// C++: No equivalent trap, defaults are evaluated per-call

// GOTCHA: Integer overflow
// Python: arbitrary precision
// C++: wraps or UB
// Use checked arithmetic or larger types for big numbers
```

---

## Psychological Survival Strategies

### Strategy #1: The Performance Demo

Dr. Notebook may not care about software engineering, but she cares about time. Her training runs take 8 hours. Her experiments queue for days.

Show her the speedup:

```
Python version: 8 hours training
C++ version: 45 minutes training

Python inference: 50ms per sample
C++ inference: 0.4ms per sample

Experiments per day (Python): 3
Experiments per day (C++): 24+
```

When experiments run faster, science happens faster. This is an argument she'll understand.

### Strategy #2: The Reproducibility Rescue

Dr. Notebook has a secret shame: she can't reproduce her own results from 6 months ago. The notebook evolved. The dependencies changed. The random seed was in a cell that got deleted.

C++ forces reproducibility:

```cpp
// Dependencies explicit and version-locked
// Build process deterministic
// No cell execution order
// No hidden global state

// Six months later: clone, build, run, same results
```

When her paper reviewer asks for the code, a C++ project with a README is more reproducible than a 200-cell notebook with mystery dependencies.

### Strategy #3: The Deployment Deal

"The notebook works. Why do I need to migrate it?"

Because notebooks don't deploy:
- Can't run on edge devices
- Can't run without Python runtime (500MB+)
- Can't run at 10,000 requests/second
- Can't run on customer's air-gapped server

C++ compiles to a standalone binary:
- Runs anywhere
- No runtime dependencies
- Handles any load
- Single file deployment

The migration isn't about code quality—it's about getting her work into production where it can have impact.

### Strategy #4: The Collaboration Carrot

Dr. Notebook works alone. Her notebook is her personal space. But the project is growing. Other people need to contribute.

Notebooks don't scale to teams:
- Merge conflicts on every cell
- No code review (what changed in this 5000-line JSON?)
- No automated testing
- Institutional knowledge in one head

C++ project structure scales:
- Clean diffs for code review
- Automated tests catch regressions
- Documentation required to compile
- Multiple contributors work in parallel

Frame the migration as enabling collaboration, not replacing her work.

---

## The Exit Strategy

Some notebooks can't be migrated. The algorithm is unclear. The domain knowledge is undocumented. The original author has graduated and moved to industry.

When migration isn't feasible:

1. **Freeze the notebook.** Pin every dependency. Document the exact execution order. Create a Docker container that preserves the environment. Pray.

2. **Treat it as a reference implementation.** Use the notebook to generate test cases. Write the C++ version from scratch using the notebook outputs as validation. Don't try to port the code—port the behavior.

3. **Wait for necessity.** Some notebooks limp along for years. When they finally break—when the dependency chain becomes unmaintainable—the rewrite becomes unavoidable. Document everything so your successor has a chance.

4. **Hire a translator.** Some consultancies specialize in this. They've seen every flavor of notebook nightmare. They know the patterns. It might be worth the money.

---

## Appendix: Quick Reference Card

### The Mindset Shift

| Python Thinking | C++ Thinking |
|-----------------|--------------|
| Run and see what happens | Compile, then run with confidence |
| Types are fluid | Types are contracts |
| Memory is infinite | Memory is finite and explicit |
| Import whatever you need | Declare dependencies upfront |
| Global variables are convenient | Explicit data flow is clear |
| The notebook is the product | The notebook is a prototype |
| If it runs, it works | If it compiles, it might work; if tests pass, it works |
| Performance is numpy's job | Performance is my job |

### The Conversion Checklist

- [ ] Algorithm extracted from notebook into functions
- [ ] Dependencies identified and version-locked
- [ ] Types made explicit (what is each variable?)
- [ ] Global state converted to function parameters
- [ ] Test cases extracted from notebook outputs
- [ ] Data loading converted to C++ (CSV, etc.)
- [ ] NumPy operations converted to Eigen
- [ ] ML model inference converted to LibTorch/ONNX
- [ ] Build system configured (CMake)
- [ ] Performance validated (should be faster)
- [ ] Results validated against Python version

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "This notebook is unmaintainable." | "Let's make this easier to reproduce." |
| "You have no tests." | "Let's capture these outputs as test cases." |
| "Global variables everywhere." | "Let's make the data flow more explicit." |
| "This won't scale." | "Let's prepare this for production deployment." |
| "I can't tell what this does." | "Can you walk me through the execution order?" |
| "Your dependencies are chaos." | "Let's document the environment for reproducibility." |
| "This is prototype code." | "This is a great starting point for the production system." |

---

*The notebook was the beginning of the journey, not the destination. Now it's time to ship.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 69%  
**Survival probability without:** 11%  
**Probability Dr. Notebook asks "but can we just run the notebook in production?":** 100%
