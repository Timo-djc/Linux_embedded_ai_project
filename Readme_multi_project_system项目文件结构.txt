multi_project_system/
├── build/(编译生成文件可删除)
├── config/
│   ├── bearing_vibration_config.hpp
│   └── fire_detection_config.hpp
├── datas/
│   ├── BearingVibration/
│   │   ├── data1_50k.csv
│   │   ├── data2_50k.csv
│   │   ├── data3_50k.csv
│   │   └── data4_50k.csv
│   └── FireDetection/
│   │   ├── fire_images/
│   │   │   ├── left/      (left_1.jpg, left_2.jpg, ...)
│   │   │   ├── center/    (center_1.jpg, center_2.jpg, ...)
│   │   │   └── right/     (right_1.jpg, right_2.jpg, ...)
│   │   └── normal_images/
│   │       ├── left/
│   │       ├── center/
│   │       └── right/
├── include/
│   ├── common/
│   │   ├── oled/
│   │   │   └── oled.hpp
│   │   ├── ConcurrentQueue.hpp
│   │   ├── FrameworkBase.hpp
│   │   ├── multi_cam.hpp
│   │   ├── ProjectDefines.hpp
│   │   ├── SystemConfig.hpp
│   │   └── UnifiedDataTypes.hpp
│   ├── frameworks/
│   │   ├── AIModelFramework.hpp
│   │   ├── DataAcquisitionFramework.hpp
│   │   ├── DataProcessingFramework.hpp
│   │   ├── DisplayLoggingFramework.hpp
│   │   └── ExtendedFeaturesFramework.hpp
│   └── projects/
│        ├── BearingVibrationProject.hpp
│        └── FireDetectionProject.hpp
├── log/
│   └── system.log
├── models/
│   └── bearing_vibration/
│   │   ├── bearing_vibration_model.onnx
│   │   └── bearing_vibration_model_ori.onnx
│   ├── fire_detection/
│   │   ├── fire_detection_model.onnx
│   │   └── fire_detection_model_ori.onnx
└── scripts/
│   ├── setup.py
│   └── multi_project.service.in
├── src/
│   ├── common/
│   │   ├─ oled/
│   │   │   ├── common/oled/u8g2/...
│   │   │   └── oled.cpp
│   │   └── multi_cam.cpp
│   ├── frameworks/
│   │   ├── AIModelFramework.cpp
│   │   ├── DataAcquisitionFramework.cpp
│   │   ├── DataProcessingFramework.cpp
│   │   ├── DisplayLoggingFramework.cpp
│   │   └── ExtendedFeaturesFramework.cpp
│   └── projects/
│   │   ├── BearingVibrationProject.cpp
│   │   └── FireDetectionProject.cpp
│   └── main.cpp
└──CMakeLists.txt