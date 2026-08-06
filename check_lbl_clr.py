import re
with open('F:/Arduino/ti59_zombie/src/rom_ml1.cpp', 'r') as f:
    content = f.read()
match = re.search(r'ML1_ROM\[5000\] = \{(.*?)\};', content, re.DOTALL)
if match:
    nums = [int(n) for n in re.findall(r'\d+', match.group(1))]
    name = {76:"LBL",71:"SBR",61:"GTO",92:"RTN",69:"OP",
            0:"0",1:"1",2:"2",3:"3",4:"4",5:"5",6:"6",7:"7",8:"8",9:"9",
            85:"+",95:"=",75:"-",65:"x",55:"/",
            22:"INV",23:"lnx",28:"log",29:"CP",
            32:"x<>t",33:"x^2",34:"sqrt",35:"1/x",
            37:"P->R",38:"sin",39:"cos",30:"tan",
            42:"STO",43:"RCL",44:"SUM",
            46:"INS",47:"CMs",48:"EXC",49:"PRD",
            52:"EE",53:"(",54:")",
            57:"ENG",58:"FIX",59:"INT",
            66:"PSE",67:"x=t",77:"x>=t",
            86:"StFlg",87:"IfFlg",88:"D.MS",89:"pi",
            90:"LST",97:"DSZ",98:"ADV",99:"PRT",
            24:"CE",25:"CLR",
            40:"IND",45:"y^x",50:"|x|",60:"DEG",70:"RAD",80:"GRAD",
            11:"A",12:"B",13:"C",14:"D",15:"E",10:"E'",
            16:"A'",17:"B'",18:"C'",19:"D'",
            21:"2nd",41:"SST",51:"BST",81:"RST",91:"R/S",93:".",94:"+/-",
            62:"PgmInd",63:"EXCInd",64:"PRDInd",72:"STOInd"}.get
    print("LBL CLR at addr 3543 (inside ML-18 scope):")
    for i in range(3543, min(3560, len(nums))):
        n = nums[i]
        print(f"  {i:4d}: {name(n):>5s}  ({n})")
    print()
    print("LBL CLR at addr 62 (ROM-wide shared):")
    for i in range(62, min(80, len(nums))):
        n = nums[i]
        print(f"  {i:4d}: {name(n):>5s}  ({n})")
    print()
    print("LBL CLR at addr 1383:")
    for i in range(1383, min(1400, len(nums))):
        n = nums[i]
        print(f"  {i:4d}: {name(n):>5s}  ({n})")
