import re
with open('F:/Arduino/ti59_zombie/src/rom_ml1.cpp', 'r') as f:
    content = f.read()
match = re.search(r'ML1_ROM\[5000\] = \{(.*?)\};', content, re.DOTALL)
if match:
    nums = [int(n) for n in re.findall(r'\d+', match.group(1))]
    # Find all LBL (76) followed by keycode 25 (CLR)
    print("Searching for LBL CLR (76, 25) in entire ROM:")
    count = 0
    for i in range(len(nums)-1):
        if nums[i] == 76 and nums[i+1] == 25:
            print(f"  Found at addr {i}")
            count += 1
    if count == 0:
        print("  NOT FOUND!")
    
    # Also check if x=t (67) at addr 3559
    print(f"\nBytes at addr 3559-3570: {nums[3559:3570]}")
    print(f"  opcode 67 at 3559: {nums[3559] == 67}")
    print(f"  next byte 25 at 3560: {nums[3560] == 25}")
