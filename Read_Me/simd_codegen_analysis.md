# AlignedVector SIMD Codegen Analysis

## Summary

The `assume_aligned()` hint in AlignedVector **does** affect code generation, producing aligned SIMD instructions (`vmovaps`) vs unaligned instructions (`vmovups`).

## Test Environment

- **CPU**: Unknown model with AVX512 support
- **Features**: AVX, AVX2, AVX512 (F, BW, CD, DQ, VL, VBMI, VBMI2, VNNI, BITALG, VPOPCNTDQ), SSE, SSE2, SSE4.1, SSE4.2, SSSE3
- **Compiler**: g++ with `-O3 -march=native -ffast-math`

## Key Findings

### 1. Vectorization Requires `-ffast-math`

Without `-ffast-math`, GCC generates **scalar** code (`vaddss` - adding one float at a time) even with alignment hints. This is because:
- Floating-point addition is not associative
- Reordering operations for vectorization changes results
- GCC is conservative about FP precision by default

**Without `-ffast-math`:**
```asm
vaddss (%rax), %xmm0, %xmm0      ; Scalar add (1 float)
vaddss -28(%rax), %xmm0, %xmm0   ; Scalar add (1 float)
vaddss -24(%rax), %xmm0, %xmm0   ; Scalar add (1 float)
...                               ; Loop unrolled, but still scalar
```

### 2. With `-ffast-math`, Alignment Hint Matters

**saxpy_aligned (with `assume_aligned()`):**
```asm
.L50:
    vmovaps (%rsi,%rax), %ymm1          ; ALIGNED load (8 floats)
    vfmadd213ps (%rdx,%rax), %ymm2, %ymm1  ; FMA: y[] + a * x[]
    vmovaps %ymm1, (%rdx,%rax)          ; ALIGNED store (8 floats)
    addq $32, %rax                       ; +32 bytes = 8 floats
```

**saxpy_no_hint (without `assume_aligned()`):**
```asm
.L76:
    vmovups (%rcx,%rax), %ymm1          ; UNALIGNED load
    vfmadd213ps (%rdx,%rax), %ymm2, %ymm1
    vmovups %ymm1, (%rdx,%rax)          ; UNALIGNED store
```

### 3. Instruction Differences

| Instruction | Meaning | Generated When |
|-------------|---------|----------------|
| `vmovaps` | Move Aligned Packed Single | With `assume_aligned()` |
| `vmovups` | Move Unaligned Packed Single | With `data()` only |
| `vaddps` | Add Packed Single (8 floats) | With `-ffast-math` |
| `vaddss` | Add Scalar Single (1 float) | Without `-ffast-math` |
| `vfmadd213ps` | Fused Multiply-Add Packed | With FMA support |

## Performance Implications

### Modern CPUs (Haswell+, 2013+)
- `vmovaps` and `vmovups` have **similar latency/throughput** when data is actually aligned
- The compiler generates different instructions, but performance difference is minimal
- Main benefit: compiler can make stronger assumptions for optimization

### Older CPUs (Sandy Bridge, Nehalem)
- Significant penalty for unaligned access
- `vmovaps` faster than `vmovups` even when data happens to be aligned

### Safety
- `vmovaps` will **segfault** if data is misaligned
- `vmovups` will work (possibly slower) on misaligned data
- Using `assume_aligned()` tells the compiler "I guarantee this is aligned"

## Recommendations

1. **Always use `-ffast-math`** (or `-funsafe-math-optimizations`) for numerical code where:
   - Exact FP ordering doesn't matter
   - You want vectorization

2. **Use `assume_aligned()` for hot loops** to:
   - Enable aligned load/store instructions
   - Help compiler optimize memory access patterns

3. **Verify alignment is maintained** through operations that might reallocate

## Code Example - Optimal Usage

```cpp
void saxpy(AlignedVector<float, 64>& y, 
           const AlignedVector<float, 64>& x, 
           float a) {
    float* yp = y.assume_aligned();
    const float* xp = x.assume_aligned();
    
    #pragma omp simd  // Optional: hint for auto-vectorization
    for (size_t i = 0; i < y.size(); ++i) {
        yp[i] += a * xp[i];
    }
}
```

Compile with:
```bash
g++ -O3 -march=native -ffast-math -fopenmp
```

## Conclusion

The 64-byte alignment in AlignedVector **is paying off** when:
1. Compiled with `-ffast-math` (or equivalent)
2. Using `assume_aligned()` in performance-critical code
3. Processing large arrays where SIMD vectorization kicks in

The `__builtin_assume_aligned()` hint successfully causes GCC to emit aligned vector instructions (`vmovaps`) instead of unaligned ones (`vmovups`).
