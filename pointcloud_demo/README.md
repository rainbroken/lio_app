# ScanForge 点云后处理最小 Demo

这是一个独立的 Qt6/C++17 点云后处理 Demo，界面参考 `../prev/preview.html`。点云显示使用 OpenGL，PCD 读取使用内置二进制解析器，避免 Qt6 与系统中 PCL/VTK 的 Qt5 依赖冲突。

当前垂直流程：

1. 导入 PCD（优先验证 `../prev/cloud_color_optimized.pcd`）；
2. OpenGL 点云显示、旋转、平移、缩放和适配视图；
3. Z 高度范围实时裁剪预览；
4. 两点距离、水平距离和高度差测量；
5. 导出裁剪后的 PCD/PLY、测量 CSV 和截图。

完整点云用于裁剪和导出，显示端最多采样 150 万点，以便测试千万点 PCD 时保持交互。当前输入要求包含 `FIELDS`、`POINTS` 和 `DATA binary`，导出的 PCD 是 ASCII 格式，适合 Demo 验证，后续可换成二进制写出。

## 构建

```bash
cd /home/rain/ros_ws/handler_ws/app/pointcloud_demo
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## 运行

```bash
./run_demo.sh
```

不带参数时只启动空界面，点击“打开 PCD”后再选择点云。也可以显式传入点云路径，让程序启动时自动加载：

```bash
./run_demo.sh ../prev/cloud_color_optimized.pcd
```
