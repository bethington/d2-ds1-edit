#!/usr/bin/env python3
"""
Script to generate INI files for all Level Types and logical groupings in LvlTypes.txt
Each INI file contains entries in the format: <tileset_id> <def> <ds1_path>
"""

import os
import csv
import glob
from pathlib import Path

def load_level_types(file_path):
    """Load Level Types data from LvlTypes.txt"""
    level_types = {}
    with open(file_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            if row['Id'] and row['Id'].strip():  # Check if Id field is not empty
                try:
                    level_id = int(row['Id'])
                    if level_id > 0:  # Skip 'None' entry with ID 0
                        level_types[level_id] = {
                            'name': row['Name'],
                            'id': level_id
                        }
                except ValueError:
                    continue  # Skip rows with invalid Id values
    return level_types

def load_level_presets(file_path):
    """Load Level Preset data from LvlPrest.txt"""
    presets = {}
    with open(file_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            if row['Def'] and row['Def'].strip():
                try:
                    def_id = int(row['Def'])
                    if def_id > 0:  # Skip 'None' entry with Def 0
                        dt1_mask = int(row['Dt1Mask']) if row['Dt1Mask'] and row['Dt1Mask'].strip() else 0
                        presets[def_id] = {
                            'name': row['Name'],
                            'def': def_id,
                            'file1': row['File1'],
                            'file2': row['File2'],
                            'dt1_mask': dt1_mask
                        }
                except ValueError:
                    continue  # Skip rows with invalid values
    return presets

def determine_tileset_id(preset):
    """Determine appropriate tileset ID based on preset characteristics"""
    name = preset['name'].lower()
    dt1_mask = preset['dt1_mask']
    
    # Cave entrance files need Wilderness tileset (ID 2) due to DT1MASK=1048835
    if 'cave entrance' in name or 'doe entrance' in name or dt1_mask == 1048835:
        return 2  # Wilderness
    
    # Regular cave files use Cave tileset (ID 3) with DT1MASK=1
    if 'cave' in name and dt1_mask == 1:
        return 3  # Cave
    
    # Graveyard uses Wilderness tileset (ID 2) - graveyrd.dt1 is part of wilderness
    if 'graveyard' in name:
        return 2  # Wilderness
    
    # Act 1 Town files
    if 'act 1 - town' in name:
        return 1  # Town
    
    # Act 1 Wilderness/Outdoors files  
    if any(x in name for x in ['wild', 'outdoors', 'border', 'cliff', 'river', 'bridge', 'stone fill', 'corral', 'fence', 'swamp', 'tree fill', 'ruin', 'fallen camp']):
        return 2  # Wilderness
    
    # Act 1 Crypt files
    if 'crypt' in name:
        return 4  # Crypt
    
    # Act 1 Monastery files
    if 'monastery' in name or 'monestary' in name:
        return 5  # Monastery
    
    # Act 1 Courtyard files
    if 'courtyard' in name:
        return 6  # Courtyard
    
    # Act 1 Barracks files
    if 'barracks' in name:
        return 7  # Barracks
    
    # Act 1 Jail files
    if 'jail' in name:
        return 8  # Jail
    
    # Act 1 Cathedral files
    if 'cathedral' in name:
        return 9  # Cathedral
    
    # Act 1 Catacombs files
    if 'catacomb' in name:
        return 10  # Catacombs
    
    # Act 1 Tristram files
    if 'tristram' in name:
        return 11  # Tristram
    
    # Act 2
    if 'act 2' in name:
        if 'town' in name:
            return 12
        elif 'sewer' in name:
            return 13
        elif 'harem' in name:
            return 14
        elif 'basement' in name:
            return 15
        elif 'desert' in name:
            return 16
        elif 'tomb' in name:
            return 17
        elif 'lair' in name:
            return 18
        elif 'arcane' in name:
            return 19
    
    # Act 3
    if 'act 3' in name:
        if 'town' in name:
            return 20
        elif 'jungle' in name:
            return 21
        elif 'kurast' in name:
            return 22
        elif 'spider' in name:
            return 23
        elif 'dungeon' in name:
            return 24
        elif 'sewer' in name:
            return 25
    
    # Act 4
    if 'act 4' in name:
        if 'town' in name:
            return 26
        elif 'mesa' in name:
            return 27
        elif 'lava' in name:
            return 28
    
    # Act 5
    if 'act 5' in name:
        if 'town' in name:
            return 29
        elif 'siege' in name:
            return 30
        elif 'barricade' in name:
            return 31
        elif 'temple' in name:
            return 32
        elif 'ice' in name:
            return 33
        elif 'baal' in name:
            return 34
        elif 'lava' in name:
            return 35
    
    # Default fallback - try to guess from file path
    file1 = preset['file1'].lower() if preset['file1'] else ''
    if 'act1/town' in file1:
        return 1
    elif 'act1/outdoors' in file1 or 'act1/caves' in file1:
        return 2
    elif 'act1/cave' in file1:
        return 3
    elif 'act1/crypt' in file1:
        return 4
    elif 'act1/court' in file1 or 'act1/monastry' in file1:
        return 5
    elif 'act1/barracks' in file1:
        return 7
    elif 'act1/cathedrl' in file1:
        return 9
    elif 'act1/catacomb' in file1:
        return 10
    
    return 1  # Default to Town tileset if can't determine

def find_ds1_files(assets_dir):
    """Find all DS1 files in the assets directory"""
    ds1_pattern = os.path.join(assets_dir, "**", "*.ds1")
    return glob.glob(ds1_pattern, recursive=True)

def get_relative_path(full_path, base_dir):
    """Get relative path from base directory"""
    return os.path.relpath(full_path, base_dir).replace('\\', '/')

def sanitize_filename(name):
    """Sanitize filename by removing invalid characters"""
    invalid_chars = '<>:"/\\|?*'
    for char in invalid_chars:
        name = name.replace(char, '_')
    return name.replace(' ', '_').replace('-', '_').lower()

def get_expected_directory_for_tileset(tileset_id, tileset_name):
    """Get the expected directory path for a given tileset"""
    name_lower = tileset_name.lower()
    
    # Define directory mappings for each tileset
    if tileset_id == 1 and 'town' in name_lower:
        return 'act1/town'
    elif tileset_id == 2 and 'wilderness' in name_lower:
        return 'act1/outdoors'
    elif tileset_id == 3 and 'cave' in name_lower:
        return 'act1/caves'
    elif tileset_id == 4 and 'crypt' in name_lower:
        return 'act1/crypt'
    elif tileset_id == 5 and 'monestary' in name_lower:
        return 'act1/monastery'
    elif tileset_id == 6 and 'courtyard' in name_lower:
        return 'act1/courtyard'
    elif tileset_id == 7 and 'barracks' in name_lower:
        return 'act1/barracks'
    elif tileset_id == 8 and 'jail' in name_lower:
        return 'act1/jail'
    elif tileset_id == 9 and 'cathedral' in name_lower:
        return 'act1/cathedral'
    elif tileset_id == 10 and 'catacombs' in name_lower:
        return 'act1/catacomb'
    elif tileset_id == 11 and 'tristram' in name_lower:
        return 'act1/tristram'
    elif tileset_id == 12 and 'act 2' in name_lower and 'town' in name_lower:
        return 'act2/town'
    elif tileset_id == 13 and 'sewer' in name_lower:
        return 'act2/sewer'
    elif tileset_id == 14 and 'harem' in name_lower:
        return 'act2/harem'
    elif tileset_id == 15 and 'basement' in name_lower:
        return 'act2/basement'
    elif tileset_id == 16 and 'desert' in name_lower:
        return 'act2/desert'
    elif tileset_id == 17 and 'tomb' in name_lower:
        return 'act2/tomb'
    elif tileset_id == 18 and 'lair' in name_lower:
        return 'act2/lair'
    elif tileset_id == 19 and 'arcane' in name_lower:
        return 'act2/arcane'
    elif tileset_id == 20 and 'act 3' in name_lower and 'town' in name_lower:
        return 'act3/town'
    elif tileset_id == 21 and 'jungle' in name_lower:
        return 'act3/jungle'
    elif tileset_id == 23 and 'spider' in name_lower:
        return 'act3/jungle'  # Spider areas are in jungle
    elif tileset_id == 24 and 'dungeon' in name_lower:
        return 'act3/dungeon'
    elif tileset_id == 25 and 'act 3' in name_lower and 'sewer' in name_lower:
        return 'act3/sewer'
    elif tileset_id == 27 and 'mesa' in name_lower:
        return 'act4/mesa'
    elif tileset_id == 28 and 'lava' in name_lower:
        return 'act4/lava'
    elif tileset_id == 29 and 'act 5' in name_lower and 'town' in name_lower:
        return 'act5/town'
    elif tileset_id == 30 and 'siege' in name_lower:
        return 'act5/siege'
    elif tileset_id == 31 and 'barricade' in name_lower:
        return 'act5/barricade'
    elif tileset_id == 32 and 'temple' in name_lower:
        return 'act5/temple'
    elif tileset_id == 33 and 'ice' in name_lower:
        return 'act5/ice'
    elif tileset_id == 34 and 'baal' in name_lower:
        return 'expansion/baallair'
    elif tileset_id == 35 and 'act 5' in name_lower and 'lava' in name_lower:
        return 'act5/lava'
    
    # Fallback: return None to indicate no specific directory filter
    return None

def should_include_file_for_tileset(file_path, tileset_id, tileset_name):
    """Check if a DS1 file should be included for a specific tileset"""
    expected_dir = get_expected_directory_for_tileset(tileset_id, tileset_name)
    
    if expected_dir is None:
        # No specific directory requirement, include all files
        return True
    
    # Normalize paths for comparison
    file_path_lower = file_path.lower().replace('\\', '/')
    expected_dir_lower = expected_dir.lower()
    
    # Check if the file is in the expected directory
    return expected_dir_lower in file_path_lower

def generate_tileset_inis(level_types, presets, ds1_files, base_dir, output_dir):
    """Generate INI files grouped by tileset type"""
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Convert DS1 file paths to relative paths for faster lookup
    relative_ds1_files = {get_relative_path(f, base_dir): f for f in ds1_files}
    
    # Group presets by tileset
    presets_by_tileset = {}
    
    for preset in presets.values():
        tileset_id = determine_tileset_id(preset)
        if tileset_id not in presets_by_tileset:
            presets_by_tileset[tileset_id] = []
        presets_by_tileset[tileset_id].append(preset)
    
    # Generate INI file for each tileset
    for tileset_id in sorted(presets_by_tileset.keys()):
        preset_list = presets_by_tileset[tileset_id]
        
        # Get tileset name
        tileset_name = level_types.get(tileset_id, {}).get('name', f'Tileset_{tileset_id}')
        
        # Create filename
        filename = f"tileset_{tileset_id:02d}_{sanitize_filename(tileset_name)}.ini"
        filepath = os.path.join(output_dir, filename)
        
        entries = []
        
        # Process each preset for this tileset
        for preset in preset_list:
            def_id = preset['def']
            
            # Check for File1 and File2 DS1 files
            for file_field in ['file1', 'file2']:
                ds1_file = preset[file_field]
                if ds1_file and ds1_file != '0':
                    # Look for this DS1 file in our assets
                    relative_path = f"assets/tiles/{ds1_file}"
                    
                    if relative_path in relative_ds1_files:
                        # Check if this file should be included for this tileset
                        if should_include_file_for_tileset(relative_path, tileset_id, tileset_name):
                            entries.append(f"{tileset_id} {def_id} {relative_path}")
                    else:
                        # Try to find a similar file (case-insensitive)
                        ds1_lower = ds1_file.lower()
                        found = False
                        
                        for rel_path in relative_ds1_files.keys():
                            if ds1_lower in rel_path.lower() or os.path.basename(ds1_lower) in rel_path.lower():
                                # Check if this file should be included for this tileset
                                if should_include_file_for_tileset(rel_path, tileset_id, tileset_name):
                                    entries.append(f"{tileset_id} {def_id} {rel_path}")
                                    found = True
                                    break
                        
                        if not found:
                            print(f"Warning: DS1 file not found or excluded: {ds1_file} for preset {preset['name']}")
        
        # Write INI file if we have entries
        if entries:
            with open(filepath, 'w', encoding='utf-8') as f:
                for entry in sorted(set(entries)):  # Remove duplicates and sort
                    f.write(entry + "\n")
            
            print(f"Generated: {filename} ({len(set(entries))} entries)")
        else:
            print(f"Warning: No DS1 files found for Tileset {tileset_id} ({tileset_name})")

def main():
    # Set up paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(script_dir, "assets")
    excel_dir = os.path.join(assets_dir, "excel")
    output_dir = os.path.join(assets_dir, "generated_inis")
    
    # Input files
    lvl_types_file = os.path.join(excel_dir, "LvlTypes.txt")
    lvl_prest_file = os.path.join(excel_dir, "LvlPrest.txt")
    
    # Check if input files exist
    if not os.path.exists(lvl_types_file):
        print(f"Error: {lvl_types_file} not found")
        return
    
    if not os.path.exists(lvl_prest_file):
        print(f"Error: {lvl_prest_file} not found")
        return
    
    print("Loading level types...")
    level_types = load_level_types(lvl_types_file)
    print(f"Loaded {len(level_types)} level types")
    
    print("Loading level presets...")
    presets = load_level_presets(lvl_prest_file)
    print(f"Loaded {len(presets)} presets")
    
    print("Finding DS1 files...")
    ds1_files = find_ds1_files(assets_dir)
    print(f"Found {len(ds1_files)} DS1 files")
    
    print("Generating tileset INI files...")
    generate_tileset_inis(level_types, presets, ds1_files, script_dir, output_dir)
    
    print(f"\nINI files generated in: {output_dir}")

if __name__ == "__main__":
    main()
