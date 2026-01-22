// tests/test_integration.cpp
// Integration test requiring a real TensorFlow C library.
// This target is only built when TF_WRAPPER_TF_STUB=OFF.

#include "tf/all.hpp"

#include <vector>
#include <cstdlib>

int main()
{
    try {
        tf::FastGraph g;

        // Const A - Use initializer_list overload (both args must be init lists)
        {
            auto a_tensor = tf::FastTensor::FromVector<float>({2}, {1.0f, 2.0f});
            (void)g.NewOperation("Const", "A")
                .SetAttrTensor("value", a_tensor.handle())
                .SetAttrType("dtype", TF_FLOAT)
                .Finish();
        }

        // Const B
        {
            auto b_tensor = tf::FastTensor::FromVector<float>({2}, {10.0f, 20.0f});
            (void)g.NewOperation("Const", "B")
                .SetAttrTensor("value", b_tensor.handle())
                .SetAttrType("dtype", TF_FLOAT)
                .Finish();
        }

        // Add A + B - Use tf::Output helper
        {
            TF_Operation* op_a = g.GetOperationOrThrow("A");
            TF_Operation* op_b = g.GetOperationOrThrow("B");
            
            (void)g.NewOperation("AddV2", "AddAB")
                .AddInput(tf::Output(op_a, 0))
                .AddInput(tf::Output(op_b, 0))
                .Finish();
        }

        tf::FastSession s(g);
        
        // Run the graph
        std::vector<tf::Fetch> fetches = {tf::Fetch{"AddAB", 0}};
        auto results = s.Run({}, fetches, {});

        if (results.size() != 1u) {
            return 1;
        }

        // Extract results using ToVector
        auto v = results[0].ToVector<float>();
        
        if (v.size() != 2u) {
            return 1;
        }

        // Verify: [1, 2] + [10, 20] = [11, 22]
        return (v[0] == 11.0f && v[1] == 22.0f) ? 0 : 1;
        
    } catch (const std::exception&) {
        return 1;
    }
}
