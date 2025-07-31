// This is a file to display benchmarking free vs. non free functions.
// There is a fair amount of code duplication here, but it's because
// I wanted  to eliminate as much overhead as possible.

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>
#include <string>
#include "pcg_random.hpp"
using namespace std;
using namespace chrono;


/** Simple 4 value vector with some operations */
struct Vec4
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;


    explicit constexpr Vec4(
        const double a_,
        const double b_,
        const double c_,
        const double d_
    ) : a(a_), b(b_), c(c_), d(d_)
    { }

    Vec4 add(const Vec4& other) const
    {
        return Vec4(
            a + other.a,
            b + other.b,
            c + other.c,
            d + other.d
        );
    }

    Vec4 subtract(const Vec4& other) const
    {
        return Vec4(
            a - other.a,
            b - other.b,
            c - other.c,
            d - other.d
        );
    }

    double dot_product(const Vec4& other) const
    {
        return (a * other.a) + (b * other.b) + (c * other.c) + (d * other.d);
    }

    void normalize()
    {
        const double dot_with_self = dot_product(*this);
        const double magnitude = sqrt(dot_with_self);
        a /= magnitude;
        b /= magnitude;
        c /= magnitude;
        d /= magnitude;
    }
};



/** The above class, but with the methods freed (via a pass structure method) */
Vec4 free_add_pass_struct(const Vec4 &v1, const Vec4 &v2)
{
    return Vec4(
        v1.a + v2.a,
        v1.b + v2.b,
        v1.c + v2.c,
        v1.d + v2.d
    );
}

Vec4 free_subtract_pass_struct(const Vec4 &v1, const Vec4 &v2)
{
    return Vec4(
        v1.a - v2.a,
        v1.b - v2.b,
        v1.c - v2.c,
        v1.d - v2.d
    );
}

double free_dot_product_pass_struct(const Vec4& v1, const Vec4 &v2)
{
    return (v1.a * v2.a) + (v1.b * v2.b) + (v1.c * v2.c) + (v1.d * v2.d);
}

void free_normalize_pass_struct(Vec4 &v)
{
    const double dot_with_self = free_dot_product_pass_struct(v, v);
    const double magnitude = sqrt(dot_with_self);
    v.a /= magnitude;
    v.b /= magnitude;
    v.c /= magnitude;
    v.d /= magnitude;
}



/** The above methods (free), but we pass in each argument instead of a struct. */
Vec4 free_add_pass_args(
    const double v1_a, const double v1_b, const double v1_c, const double v1_d,
    const double v2_a, const double v2_b, const double v2_c, const double v2_d
) {
    return Vec4(
        v1_a + v2_a,
        v1_b + v2_b,
        v1_c + v2_c,
        v1_d + v2_d
    );
}

Vec4 free_subtract_pass_args(
    const double v1_a, const double v1_b, const double v1_c, const double v1_d,
    const double v2_a, const double v2_b, const double v2_c, const double v2_d
) {
    return Vec4(
        v1_a - v2_a,
        v1_b - v2_b,
        v1_c - v2_c,
        v1_d - v2_d
    );
}

double free_dot_product_pass_args(
    const double v1_a, const double v1_b, const double v1_c, const double v1_d,
    const double v2_a, const double v2_b, const double v2_c, const double v2_d
)
{
    return (v1_a * v2_a) + (v1_b * v2_b) + (v1_c * v2_c) + (v1_d * v2_d);
}

void free_normalize_pass_args(
    double &v_a, double &v_b, double &v_c, double &v_d
)
{
    const double dot_with_self = free_dot_product_pass_args(v_a, v_b, v_c, v_d, v_a, v_b, v_c, v_d);
    const double magnitude = sqrt(dot_with_self);
    v_a /= magnitude;
    v_b /= magnitude;
    v_c /= magnitude;
    v_d /= magnitude;
}



/** Random generator object */
class RNG
{
private:
    pcg32 rng_engine;

public:
    explicit RNG(const std::string& seed_str)
    {
        std::seed_seq seed(seed_str.begin(), seed_str.end());
        rng_engine.seed(seed);
    }

    double num(
        const double a = -1.0,
        const double b = 1.0
    ) {
        std::uniform_real_distribution<double> dist(a, b);
        return dist(rng_engine);
    }

    Vec4 vec4(
        const double a = -1.0,
        const double b = 1.0
    ) {
        return Vec4(
            num(a, b),
            num(a, b),
            num(a, b),
            num(a, b)
        );
    }
};



/*== The benchmark functions ==*/
Vec4 bucket_vec4(0.0, 0.0, 0.0, 0.0);           // Trick to make sure compiler doesn't optimize away stuff
double bucket_double = 0.0;                     // Trick to make sure compiler doesn't optimize away stuff


// Add
int64_t benchmark_member_add(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const Vec4 a = rng.vec4();
        const Vec4 b = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        const Vec4 result = a.add(b);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = result;
    }

    return total_duration;
}
int64_t benchmark_free_add_pass_struct(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const Vec4 a = rng.vec4();
        const Vec4 b = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        const Vec4 result = free_add_pass_struct(a, b);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = result;
    }

    return total_duration;
}
int64_t benchmark_free_add_pass_args(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const double a = rng.num();
        const double b = rng.num();
        const double c = rng.num();
        const double d = rng.num();
        const double w = rng.num();
        const double x = rng.num();
        const double y = rng.num();
        const double z = rng.num();

        // Do the measurement
        const auto start = steady_clock::now();
        const Vec4 result = free_add_pass_args(a, b, c, d, w, x, y, z);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = result;
    }

    return total_duration;
}


// Subtract
int64_t benchmark_member_subtract(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const Vec4 a = rng.vec4();
        const Vec4 b = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        const Vec4 result = a.subtract(b);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = result;
    }

    return total_duration;
}
int64_t benchmark_free_subtract_pass_struct(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const Vec4 a = rng.vec4();
        const Vec4 b = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        const Vec4 result = free_subtract_pass_struct(a, b);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = result;
    }

    return total_duration;
}
int64_t benchmark_free_subtract_pass_args(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const double a = rng.num();
        const double b = rng.num();
        const double c = rng.num();
        const double d = rng.num();
        const double w = rng.num();
        const double x = rng.num();
        const double y = rng.num();
        const double z = rng.num();

        // Do the measurement
        const auto start = steady_clock::now();
        const Vec4 result = free_subtract_pass_args(a, b, c, d, w, x, y, z);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = result;
    }

    return total_duration;
}


// Dot Product
int64_t benchmark_member_dot_product(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const Vec4 a = rng.vec4();
        const Vec4 b = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        const double result = a.dot_product(b);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_double = result;
    }

    return total_duration;
}
int64_t benchmark_free_dot_product_pass_struct(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const Vec4 a = rng.vec4();
        const Vec4 b = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        const double result = free_dot_product_pass_struct(a, b);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_double = result;
    }

    return total_duration;
}
int64_t benchmark_free_dot_product_pass_args(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        const double a = rng.num();
        const double b = rng.num();
        const double c = rng.num();
        const double d = rng.num();
        const double w = rng.num();
        const double x = rng.num();
        const double y = rng.num();
        const double z = rng.num();

        // Do the measurement
        const auto start = steady_clock::now();
        const double result = free_dot_product_pass_args(a, b, c, d, w, x, y, z);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_double = result;
    }

    return total_duration;
}



// Normalize
int64_t benchmark_member_normalize(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        Vec4 a = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        a.normalize();
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = a;
    }

    return total_duration;
}
int64_t benchmark_free_normalize_pass_struct(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        Vec4 a = rng.vec4();

        // Do the measurement
        const auto start = steady_clock::now();
        free_normalize_pass_struct(a);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = a;
    }

    return total_duration;
}
int64_t benchmark_free_normalize_pass_args(const string &rng_seed, const int run_count, const int num_vectors)
{
    // Make a different RNG for each run count
    RNG rng(rng_seed + to_string(run_count));

    int64_t total_duration = 0;

    // Do the benchmark
    for (int i = 0; i < num_vectors; i++)
    {
        double a = rng.num();
        double b = rng.num();
        double c = rng.num();
        double d = rng.num();

        // Do the measurement
        const auto start = steady_clock::now();
        free_normalize_pass_args(a, b, c, d);
        const auto end = steady_clock::now();

        // Record the measurement
        const auto duration = duration_cast<nanoseconds>(end - start);
        total_duration += duration.count();

        // Prevent compiler from optimizing away the call
        bucket_vec4 = Vec4(a, b, c, d);
    }

    return total_duration;
}



using benchmark_function_signature = decltype(&benchmark_member_add);
struct Benchmark
{
    string title;
    benchmark_function_signature function;
};




int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        cout << "Usage:" << endl;
        cout << "  " << argv[0] << " <rng_seed:string> <num_runs:int> <num_vectors_per_run:int>" << endl;
        return 1;
    }

    // Parse some command line arguments
    const string rng_seed = argv[1];
    const int num_runs = stoi(argv[2]);
    const int num_vectors_per_run = stoi(argv[3]);

    cout << "RNG Seed: " << rng_seed << endl;
    cout << "No. of Runs: " << num_runs << endl;
    cout << "No. of Vectors per Run: " << num_vectors_per_run << endl;
    cout << "(all units are in nanoseconds)" << endl;
    cout << endl;

    auto benchmarks = vector<Benchmark>();
    benchmarks.push_back({"add()[member]", benchmark_member_add});
    benchmarks.push_back({"add()[free][pass_struct]", benchmark_free_add_pass_struct});
    benchmarks.push_back({"add()[free][pass_args]", benchmark_free_add_pass_args});
    benchmarks.push_back({"subtract()[member]", benchmark_member_subtract});
    benchmarks.push_back({"subtract()[free][pass_struct]", benchmark_free_subtract_pass_struct});
    benchmarks.push_back({"subtract()[free][pass_args]", benchmark_free_subtract_pass_args});
    benchmarks.push_back({"dot_product()[member]", benchmark_member_dot_product});
    benchmarks.push_back({"dot_product()[free][pass_struct]", benchmark_free_dot_product_pass_struct});
    benchmarks.push_back({"dot_product()[free][pass_args]", benchmark_free_dot_product_pass_args});
    benchmarks.push_back({"normalize()[member]", benchmark_member_normalize});
    benchmarks.push_back({"normalize()[free][pass_struct]", benchmark_free_normalize_pass_struct});
    benchmarks.push_back({"normalize()[free][pass_args]", benchmark_free_normalize_pass_args});

    // Print out the titles
    for (const auto &b: benchmarks)
    {
        cout << b.title << " ";
    }
    cout << endl;

    // Do the tests
    for (int run = 0; run < num_runs; run++)
    {
        for (const auto &b: benchmarks)
        {
            const int64_t time_taken = b.function(rng_seed, run, num_vectors_per_run);
            cout << time_taken << " " << flush;
        }
        cout << endl;
    }

    cout << "--------------------" << endl;
    cout << endl;
    return 0;
}
