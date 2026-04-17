import subprocess


reg1 = int(subprocess.check_output(
    ["rdmsr", "-p0", "0x1ae"]).strip(), 16)
reg2 = int(subprocess.check_output(
    ["rdmsr", "-p0", "0x1ad"]).strip(), 16)
for i in range(8):
    num_cores = (reg1 >> (8*i)) & 0xff
    ratio = (reg2 >> (8*i)) & 0xff
    print(f"{num_cores} cores: {ratio*100} MHz")