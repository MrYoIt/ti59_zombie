rom = []
with open('src/rom_ml1.cpp', 'r') as f:
    content = f.read()
import re
match = re.search(r'ML1_ROM\[5000\] = \{(.*?)\};', content, re.DOTALL)
if match:
    nums = re.findall(r'\d+', match.group(1))
    rom = [int(n) for n in nums]

# Check what's at LBL 95 (addr 76)
print("At address 76 (LBL 95):")
for i in range(76, 100):
    if i < len(rom):
        print(f"  {i}: {rom[i]}")

# Find all references to label 95 (SBR 95 or GTO 95)
print("\nReferences to label 95 (SBR/GTO 95):")
for i in range(len(rom)-1):
    if (rom[i] == 71 or rom[i] == 61) and rom[i+1] == 95:  # SBR=71, GTO=61
        print(f"  {i}: {'SBR' if rom[i]==71 else 'GTO'} 95")