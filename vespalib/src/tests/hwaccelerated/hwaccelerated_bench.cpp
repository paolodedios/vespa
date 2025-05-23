// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/hwaccelerated/iaccelerated.h>
#include <vespa/vespalib/hwaccelerated/generic.h>
#include <vespa/vespalib/util/time.h>
#include <cinttypes>

using namespace vespalib;

template<typename T>
std::vector<T> createAndFill(size_t sz) {
    std::vector<T> v(sz);
    for (size_t i(0); i < sz; i++) {
        v[i] = rand()%128;
    }
    return v;
}

template<typename T>
void
benchmarkEuclideanDistance(const hwaccelerated::IAccelerated & accel, size_t sz, size_t count) {
    srand(1);
    std::vector<T> a = createAndFill<T>(sz);
    std::vector<T> b = createAndFill<T>(sz);
    steady_time start = steady_clock::now();
    double sumOfSums(0);
    for (size_t j(0); j < count; j++) {
        double sum = accel.squaredEuclideanDistance(&a[0], &b[0], sz);
        sumOfSums += sum;
    }
    duration elapsed = steady_clock::now() - start;
    printf("sum=%f of N=%zu and vector length=%zu took %" PRId64 "\n", sumOfSums, count, sz, count_ms(elapsed));
}

void
benchMarkEuclidianDistance(const hwaccelerated::IAccelerated & accelerator, size_t sz, size_t count) {
    printf("double : ");
    benchmarkEuclideanDistance<double>(accelerator, sz, count);
    printf("float  : ");
    benchmarkEuclideanDistance<float>(accelerator, sz, count);
    printf("int8_t : ");
    benchmarkEuclideanDistance<int8_t>(accelerator, sz, count);
}

int main(int argc, char *argv[]) {
    int length = 1000;
    int count = 1000000;
    if (argc > 1) {
        length = atol(argv[1]);
    }
    if (argc > 2) {
        count = atol(argv[2]);
    }
    printf("%s %d %d\n", argv[0], length, count);
    printf("Squared Euclidian Distance - Generic\n");
    benchMarkEuclidianDistance(hwaccelerated::GenericAccelerator(), length, count);
    printf("Squared Euclidian Distance - Optimized for this cpu\n");
    benchMarkEuclidianDistance(hwaccelerated::IAccelerated::getAccelerator(), length, count);
    return 0;
}
