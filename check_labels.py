import re

with open('F:/Arduino/ti59_zombie/src/rom_ml1.cpp', 'rb') as f:
    data = f.read()

match = re.search(rb'ML1_ROM\[5000\] = \{(.*?)\};', data, re.DOTALL)
if match:
    nums = list(map(int, re.findall(rb'\d+', match.group(1))))
    
    # Check ML-01 (addr 54, len 189)
    print('=== ML-01 (addr 54-242) ===')
    labels = {11:'A',12:'B',13:'C',14:'D',15:'E',16:"A'",17:"B'",18:"C'",19:"D'",10:"E'",95:'='}
    for i in range(54, 243):
        if nums[i] == 76:  # KC_LBL
            lbl = nums[i+1]
            print(f'  LBL {labels.get(lbl, lbl)} ({lbl}) at addr {i} (offset {i-54})')
    
    # Check ML-18 (addr 3480, len 171)
    print('=== ML-18 (addr 3480-3650) ===')
    for i in range(3480, 3651):
        if nums[i] == 76:  # KC_LBL
            lbl = nums[i+1]
            print(f'  LBL {labels.get(lbl, lbl)} ({lbl}) at addr {i} (offset {i-3480})')