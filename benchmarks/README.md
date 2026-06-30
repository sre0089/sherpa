# Index benchmark

The manual benchmark compares one full index with repeated unchanged incremental indexes. It uses a
temporary database and does not modify the target repository or Sherpa's normal cache.

```sh
cmake --preset dev -DSHERPA_BUILD_BENCHMARKS=ON
cmake --build --preset dev --target sherpa_index_benchmark
./build/dev/sherpa_index_benchmark /path/to/large/cpp/repository 5
```

Output is CSV so results can be retained and compared between commits. Record the repository commit,
Sherpa commit, compiler, build type, operating system, CPU, and storage type with published results.
The benchmark intentionally reports phases separately: scanning still hashes every supported file,
while unchanged runs should report zero parsed files.
