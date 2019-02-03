# -*- coding: utf-8 -*-
import collections as col

def find_2nd(string, substring):
   return string.find(substring, string.find(substring) + 1)

common = {}
msa = {}
address = col.defaultdict(dict)
f = open('log_msa')
cnt = -1
for line in f.readlines():
    cnt +=1
    command = line[:line.find(" ")]
    indx = line.find("0")
    c_address = line[indx:-1]
    if 'MSA' in line:
        pass
    elif indx != -1:
        if c_address not in address.keys():
            address[c_address][command] = str(1) + "  " + str(cnt) + " "
        elif command not in address[c_address]:
            address[c_address][command] = str(1) + "  " + str(cnt) + " "
        else:        
            k = int(address[c_address][command][0: address[c_address][command].find(" ")])
            k += 1
            address[c_address][command] = str(k) + address[c_address][command][address[c_address][command].find(" "):] + " " + str(cnt)
    elif command not in common.keys() and command not in msa.keys():
        if '.df' in line:        
            msa[command] = 1
        else:
            common[command] = 1
    else:
        if '.df' in line:        
            msa[command] += 1
        else:
            common[command] += 1
f.close()
out = open('log_msa_commands', 'w')
acc_common = 0
acc_msa = 0
for k, v in msa.items():
    out.write(str(k) + ': ' + str(v) + '\n')
    acc_msa += v
for k, v in common.items():
    out.write(str(k) + ': ' + str(v) + '\n')
    acc_common += v
out.write('\ntotal: '+ str(acc_common+acc_msa))
out.write('\nMSA%: ' + str(acc_msa*100/(acc_common+acc_msa)))
out.close()

with open('log_msa_addresses_order','w') as f:
    for key, values in address.items():
        f.write(key + '\n')
        f.write("\n\n".join(["{}: {}".format(value_key, digit) for value_key, digit in values.items()]) + '\n\n\n')
        f.write('\n')
f.close()

with open('log_msa_addresses','w') as f:
    for key, values in address.items():
        f.write(key + '\n')
        f.write("\n".join(["{}: {}".format(value_key, digit[0:find_2nd(digit, " ")]) for value_key, digit in values.items()]) + '\n\n')
        f.write('\n')
