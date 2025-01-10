# HD3L: High-Speed Dataloader for Distributed Deep Learning

HD3L is a high-performance data loading library designed to accelerate distributed deep learning tasks. By optimizing data preprocessing and transfer, it minimizes bottlenecks during training, enhancing overall efficiency.

## Features

- **High Throughput Data Loading**: Employs parallel data preprocessing and efficient I/O operations to ensure rapid data loading.
- **Distributed Training Support**: Optimized for multi-node training environments, ensuring data synchronization and load balancing across nodes.
- **Seamless Integration**: Provides a straightforward API compatible with popular deep learning frameworks such as PyTorch and TensorFlow.

## Installation

Ensure the following dependencies are installed:

- C++17 compiler
- CMake 3.10 or higher

To build HD3L:

```bash
git clone https://github.com/sysysyww/HD3L-High-Speed-Dataloader-for-Distributed-Deep-Learning.git
cd HD3L-High-Speed-Dataloader-for-Distributed-Deep-Learning
mkdir build && cd build
cmake ..
make
```

## Usage
Incorporate HD3L into your deep learning training scripts as follows:

1. Include HD3L Headers:
Ensure your project includes the HD3L headers:

```C++
#include "hd3l/dataloader.h"
```

2. Initialize the Dataloader:

Configure the dataloader with your dataset parameters:

```C++
HD3L::DataLoader dataloader(dataset_path, batch_size, num_workers);
```

3.Iterate Over Data:

Use the dataloader in your training loop:

```C++
for (auto& batch : dataloader) {
    // Training code here
}
```
