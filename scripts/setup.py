#!/usr/bin/env python3
"""
多项目系统安装脚本
支持火焰检测和轴承振动检测项目
"""

import os
import sys
import subprocess
import argparse
import shutil
from pathlib import Path

class MultiProjectInstaller:
    def __init__(self, project_type="vibration"):
        self.project_type = project_type.lower()
        self.project_name = "多项目检测系统"
        self.version = "2.0.0"
        
        # 目录结构
        self.base_dir = Path.home() / "multi_project_system"
        self.config = {
            "models_dir": self.base_dir / "models",
            "data_dir": self.base_dir / "data",
            "logs_dir": self.base_dir / "logs",
            "config_dir": self.base_dir / "config",
            "scripts_dir": self.base_dir / "scripts"
        }
    
    def check_dependencies(self):
        """检查系统依赖"""
        print("检查系统依赖...")
        
        dependencies = [
            ("gcc", "GCC编译器"),
            ("cmake", "CMake构建系统"),
            ("python3", "Python 3"),
            ("opencv", "OpenCV库"),
            ("wiringpi", "树莓派GPIO库")
        ]
        
        for cmd, name in dependencies:
            try:
                subprocess.run([cmd, "--version"], 
                              stdout=subprocess.DEVNULL, 
                              stderr=subprocess.DEVNULL)
                print(f"✓ {name}")
            except FileNotFoundError:
                print(f"✗ {name} 未找到")
                return False
        
        return True
    
    def install_system_deps(self):
        """安装系统依赖包"""
        print("安装系统依赖包...")
        
        packages = [
            "build-essential",
            "cmake",
            "git",
            "libopencv-dev",
            "python3-opencv",
            "libonnxruntime-dev",
            "wiringpi",
            "libusb-1.0-0-dev",
            "python3-pip"
        ]
        
        try:
            subprocess.run(["sudo", "apt", "update"], check=True)
            subprocess.run(["sudo", "apt", "install", "-y"] + packages, check=True)
            print("✓ 系统依赖安装完成")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ 安装失败: {e}")
            return False
    
    def setup_directories(self):
        """创建目录结构"""
        print("创建目录结构...")
        
        for name, path in self.config.items():
            path.mkdir(parents=True, exist_ok=True)
            print(f"✓ 创建目录: {path}")
        
        # 创建项目特定目录
        if self.project_type == "fire":
            (self.config["models_dir"] / "fire_detection").mkdir(exist_ok=True)
            (self.config["data_dir"] / "fire_images").mkdir(exist_ok=True)
        elif self.project_type == "vibration":
            (self.config["models_dir"] / "bearing_vibration").mkdir(exist_ok=True)
            (self.config["data_dir"] / "vibration_data").mkdir(exist_ok=True)
        
        return True
    
    def download_models(self):
        """下载预训练模型"""
        print("下载AI模型...")
        
        model_urls = {
            "fire": {
                "url": "https://example.com/models/fire_detection.onnx",
                "path": self.config["models_dir"] / "fire_detection" / "best.onnx"
            },
            "vibration": {
                "url": "https://example.com/models/vibration_model.onnx",
                "path": self.config["models_dir"] / "bearing_vibration" / "vibration_model.onnx"
            }
        }
        
        if self.project_type not in model_urls:
            print(f"⚠ 未知项目类型: {self.project_type}")
            return False
        
        url = model_urls[self.project_type]["url"]
        path = model_urls[self.project_type]["path"]
        
        try:
            import urllib.request
            print(f"正在下载模型: {url}")
            urllib.request.urlretrieve(url, path)
            print(f"✓ 模型下载完成: {path}")
            return True
        except Exception as e:
            print(f"✗ 模型下载失败: {e}")
            print("请手动下载模型文件")
            return False
    
    def build_system(self):
        """编译系统"""
        print("编译系统...")
        
        build_dir = self.base_dir / "build"
        build_dir.mkdir(exist_ok=True)
        
        # 切换到构建目录
        os.chdir(build_dir)
        
        # 配置CMake
        cmake_cmd = ["cmake", ".."]
        
        if self.project_type == "fire":
            cmake_cmd.append("-DFIRE_DETECTION_PROJECT=ON")
            cmake_cmd.append("-DBEARING_VIBRATION_PROJECT=OFF")
        else:
            cmake_cmd.append("-DFIRE_DETECTION_PROJECT=OFF")
            cmake_cmd.append("-DBEARING_VIBRATION_PROJECT=ON")
        
        try:
            # 运行CMake
            subprocess.run(cmake_cmd, check=True)
            
            # 编译
            cpu_count = os.cpu_count() or 2
            subprocess.run(["make", "-j", str(cpu_count)], check=True)
            
            print("✓ 系统编译完成")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ 编译失败: {e}")
            return False
    
    def setup_service(self):
        """配置系统服务"""
        print("配置系统服务...")
        
        service_content = f"""[Unit]
Description=Multi-Project Detection System ({self.project_type})
After=network.target

[Service]
Type=simple
User={os.getenv('USER')}
WorkingDirectory={self.base_dir}
ExecStart={self.base_dir}/build/multi_project_system --mode normal
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
"""
        
        service_path = Path("/lib/systemd/system") / "multi_project.service"
        
        try:
            # 需要sudo权限
            subprocess.run(["sudo", "tee", str(service_path)], 
                          input=service_content.encode(),
                          check=True)
            
            subprocess.run(["sudo", "systemctl", "daemon-reload"], check=True)
            subprocess.run(["sudo", "systemctl", "enable", "multi_project.service"], 
                          check=True)
            
            print("✓ 系统服务配置完成")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ 服务配置失败: {e}")
            return False
    
    def install_python_packages(self):
        """安装Python包"""
        print("安装Python依赖包...")
        
        requirements = [
            "numpy",
            "opencv-python",
            "onnxruntime",
            "pyserial",
            "pandas",
            "scikit-learn",
            "matplotlib"
        ]
        
        try:
            subprocess.run([sys.executable, "-m", "pip", "install"] + requirements, 
                          check=True)
            print("✓ Python包安装完成")
            return True
        except subprocess.CalledProcessError as e:
            print(f"✗ Python包安装失败: {e}")
            return False
    
    def run(self):
        """运行安装流程"""
        print(f"开始安装 {self.project_name} - {self.project_type}项目")
        print("=" * 50)
        
        steps = [
            ("检查依赖", self.check_dependencies),
            ("安装系统依赖", self.install_system_deps),
            ("创建目录", self.setup_directories),
            ("下载模型", self.download_models),
            ("安装Python包", self.install_python_packages),
            ("编译系统", self.build_system),
            ("配置服务", self.setup_service)
        ]
        
        for step_name, step_func in steps:
            print(f"\n[{step_name}]")
            if not step_func():
                print(f"安装失败: {step_name}")
                return False
        
        print("\n" + "=" * 50)
        print(f"安装完成! {self.project_name}")
        print(f"项目类型: {self.project_type}")
        print(f"安装目录: {self.base_dir}")
        print("\n启动命令:")
        print("  sudo systemctl start multi_project.service")
        print("  sudo systemctl status multi_project.service")
        print("\n查看日志:")
        print(f"  tail -f {self.config['logs_dir']}/system.log")
        
        return True

def main():
    parser = argparse.ArgumentParser(description="多项目检测系统安装程序")
    parser.add_argument("--project", choices=["fire", "vibration"], 
                       default="vibration", help="选择项目类型")
    parser.add_argument("--skip-deps", action="store_true", 
                       help="跳过依赖检查")
    parser.add_argument("--skip-build", action="store_true", 
                       help="跳过编译")
    
    args = parser.parse_args()
    
    installer = MultiProjectInstaller(args.project)
    
    # 设置跳过标志
    if args.skip_deps:
        installer.check_dependencies = lambda: True
        installer.install_system_deps = lambda: True
    
    if args.skip_build:
        installer.build_system = lambda: True
    
    success = installer.run()
    
    if not success:
        print("\n安装失败!")
        sys.exit(1)
    
    print("\n安装成功!")
    sys.exit(0)

if __name__ == "__main__":
    main()