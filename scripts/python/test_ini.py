#!/usr/bin/env python3
"""
Test launcher for generated INI files
"""

import os
import subprocess
import sys

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_ini.py <ini_filename>")
        print("\nAvailable INI files:")
        
        ini_dir = "assets/generated_inis"
        if os.path.exists(ini_dir):
            for file in sorted(os.listdir(ini_dir)):
                if file.endswith('.ini'):
                    print(f"  {file}")
        return
    
    ini_file = sys.argv[1]
    ini_path = f"assets/generated_inis/{ini_file}"
    
    if not os.path.exists(ini_path):
        print(f"Error: {ini_path} not found")
        return
    
    # Launch DS1Edit with the INI file
    cmd = f"bin\\win_ds1edit_debug.exe {ini_path}"
    print(f"Launching: {cmd}")
    
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        print("STDOUT:")
        print(result.stdout)
        if result.stderr:
            print("STDERR:")
            print(result.stderr)
        print(f"Exit code: {result.returncode}")
    except Exception as e:
        print(f"Error running command: {e}")

if __name__ == "__main__":
    main()
