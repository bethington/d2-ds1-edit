#!/usr/bin/env python3
"""
Rename tileset INI files from tileset_XX_act_X___name.ini format to actX_name.ini format
"""

import os
import glob
import re

def rename_tileset_files():
    # Source and destination directories
    source_dir = "assets/generated_inis"
    dest_dir = "assets"
    
    # Find all tileset files
    pattern = os.path.join(source_dir, "tileset_*.ini")
    files = glob.glob(pattern)
    
    print(f"Found {len(files)} tileset files to rename:")
    
    for filepath in sorted(files):
        filename = os.path.basename(filepath)
        
        # Extract components using regex
        # Pattern: tileset_XX_act_X___name.ini
        match = re.match(r'tileset_(\d+)_act_(\d+)___(.+)\.ini$', filename)
        
        if match:
            tileset_id, act_num, area_name = match.groups()
            
            # Create new filename: actX_name.ini
            new_filename = f"act{act_num}_{area_name}.ini"
            new_filepath = os.path.join(dest_dir, new_filename)
            
            print(f"  {filename} -> {new_filename}")
            
            # Copy the file with new name
            try:
                with open(filepath, 'r', encoding='utf-8') as src:
                    content = src.read()
                
                with open(new_filepath, 'w', encoding='utf-8') as dst:
                    dst.write(content)
                    
                print(f"    ✓ Created {new_filepath}")
                
            except Exception as e:
                print(f"    ✗ Error processing {filename}: {e}")
        else:
            print(f"  ✗ Skipped {filename} (doesn't match expected pattern)")
    
    print("\nRename operation completed!")

if __name__ == "__main__":
    rename_tileset_files()
