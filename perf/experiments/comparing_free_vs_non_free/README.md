This is the directory that contains measurements for @define-private-public's
(def.pri.pub@gmail.com) "Free Function" exercise.

One component is a simple C++ langauge benchmark (with `Makefile`).  These files
are:

- `benchmark.cpp` (and Makefile)
- `Vec4_benchmark_results.xlsx`         <- data measurements
- `Vec4_benchmark_analysis.ipynb`       <- Jupyter notebook analysis

The other is where the Synfig program (renderer) was used to conjunction with
Synfig's suite of sample `.sif` files.  The `Color::clamped()` method was
transofrmed inton a free function (via various methods) and then measured
against the original member funciton.  The code changes can be found on the
branches:

- `ffe_Color_clamped_friend_function`
- `ffe_Color_clamped_pass_arguments`
- `ffe_Color_clamped_public_members`

Files:

- `synfig_clamped_linux_measurements/*`     <- a bunch of JSON files with measurements
- `synfig_analysis.ipynb`                   <- Jupyter notebook analysis



Check out https://16bpp.net/ for more.
