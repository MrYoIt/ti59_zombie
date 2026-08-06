rom = []
with open('src/rom_ml1.cpp', 'r') as f:
    content = f.read()
import re
match = re.search(r'ML1_ROM\[5000\] = \{(.*?)\};', content, re.DOTALL)
if match:
    nums = re.findall(r'\d+', match.group(1))
    rom = [int(n) for n in nums]
    print(f'ROM size: {len(rom)}')
    
    start = 54
    end = 54 + 189
    print(f'Program 1: {start} to {end}')
    
    labels = {11:'A',12:'B',13:'C',14:'D',15:'E',16:"A'",17:"B'",18:"C'",19:"D'",10:"E'"}
    for i in range(start, end):
        if rom[i] == 76 and i+1 < len(rom):
            label = rom[i+1]
            print(f'  LBL {labels.get(label, str(label))} at addr {i} (offset {i-start})')