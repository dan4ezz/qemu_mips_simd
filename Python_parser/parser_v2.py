# -*- coding: utf-8 -*-
import collections as col
import sys
import os

only_main = False

if len(sys.argv) == 1:
    path_to_log = './log_msa'
    path_to_metrics = './metrics'
elif len(sys.argv) == 2:
    path_to_log = sys.argv[1] + '/log_msa'
    path_to_metrics = './metrics'
elif len(sys.argv) == 3:
    path_to_log = sys.argv[1] + '/log_msa'
    path_to_metrics = sys.argv[2]    
    path_to_metrics += '/metrics'
elif len(sys.argv) == 4:
    path_to_log = sys.argv[1] + '/log_msa'
    name_of_metrics_folder = sys.argv[3]
    path_to_metrics = sys.argv[2]
    path_to_metrics += '/'   
    path_to_metrics += name_of_metrics_folder
elif len(sys.argv) == 5:
    path_to_log = sys.argv[1] + '/log_msa'
    name_of_metrics_folder = sys.argv[3]
    path_to_metrics = sys.argv[2]
    path_to_metrics += '/'   
    path_to_metrics += name_of_metrics_folder
    only_main = True

if only_main == True:
    main_cnt = -1
else:
    file = open(path_to_metrics + '/main_cnt', 'r')
    main_cnt = int(file.readline())
    
if not os.path.exists(path_to_metrics):
    os.mkdir(path_to_metrics)
    print('Directory', path_to_metrics, 'created')
else:    
    print('Directory', path_to_metrics, 'already exists')
    
if os.path.exists(path_to_log):
    f = open(path_to_log)    
    print('Log file loaded')    
else:
    print('Log file was not loaded')
    sys.exit(0)

common = {}
msa = {}
msaldst = {}
time_localization = col.defaultdict(list)
ldst_cnt = 0
clasters = {}
aligned_addresses = 0
unaligned_addresses = 0
cnt = -1

#Команды load и store
ldst_list = ['LB', 'LBU', 'LWU', 'LD', 'LLD', 'LDL', 'LDR', 'LDPC', 'LWPC', 'LWE', 'LW', 'LHE', 'LH', 'LHUE',
             'LHU', 'LBE', 'LBUE', 'LWLE', 'LWL', 'LWRE', 'LWR', 'LLE', 'LL', 'SD', 'SDL', 'SDR', 'SWE', 'SW',
             'SHE', 'SH', 'SBE', 'SB', 'SWLE', 'SWL', 'SWRE', 'SWR', 'SCD', 'SCE', 'SC', 'LWC1', 'SWC1', 'LDC1',
             'SDC1', 'MSA_LD_B', 'MSA_LD_H', 'MSA_LD_W', 'MSA_LD_D', 'MSA_ST_B', 'MSA_ST_H', 'MSA_ST_W',
             'MSA_ST_D']

for line in f.readlines():
    if only_main == True:
        main_cnt += 1
    else:
        if main_cnt > cnt:
            cnt += 1
        else:
            cnt +=1
            command = line[:line.find(" ")]
            indx = line.find("0")
            c_address = line[indx:-1]
            if 'MSA:' in line:
                pass
            if command in ldst_list:
                ldst_cnt += 1
            else:
                pass
            if indx != -1:
                #Временная локализация
                if c_address not in time_localization.keys():
                    time_localization[c_address].append(0)
                    time_localization[c_address].append(0)
                    for k, v in time_localization.items():
                        if time_localization[k][1] == 0:
                            v[0] += 1
                        else:
                            pass
                else:
                    time_localization[c_address][1] = 1
                    for k, v in time_localization.items():
                        if time_localization[k][1] == 0:
                            v[0] += 1
                        else:
                            pass
                #Оценка выравненных и невыравненных данных
                if int(c_address, 16) % 16 == 0:
                    aligned_addresses += 1
                else:
                    unaligned_addresses += 1
                #Пространственная локализация    
                claster = int(c_address, 16) // 32
                if claster not in clasters.keys():
                    clasters[claster] = 1
                else:
                    clasters[claster] += 1
            #Оценка векторизации
            if command not in common.keys() and command not in msa.keys() and command not in msaldst.keys():
                if 'MSA' in line:
                    if 'LD' in line or 'ST' in line:
                        msaldst[command] = 1
                    elif 'MULV_df' in line or 'DIV' in line or 'SUB' in line or 'ADD' in line:          
                        msa[command] = 1
                else:
                    common[command] = 1
            else:
                if 'MSA' in line:
                    if 'LD' in line or 'ST' in line:
                        msaldst[command] += 1
                    elif 'MULV_df' in line or 'DIV' in line or 'SUB' in line or 'ADD' in line:
                        msa[command] += 1
                else:
                    common[command] += 1
if only_main == True:
    out = open(path_to_metrics + '/main_cnt', 'w')    
    out.write(str(main_cnt))
else:
    out = open(path_to_metrics + '/msa_metric', 'w')
    acc_common = 0
    acc_msa = 0
    acc_msaldst = 0
    for k, v in msa.items():
        acc_msa += v
    for k, v in msaldst.items():
        acc_msaldst += v
    for k, v in common.items():
        acc_common += v
    out.write('Среднее число векторных операций на один доступ к данным: ' + str((acc_msa)/(acc_msaldst)))
    out.close()
    
    out = open(path_to_metrics + '/time_localization_metric', 'w')
    out.write('Временная локализация данных: количество уникальных адресов, которые запрашивается между двумя соседними запросами к адресу X, включая сам адрес X.\n')
    for k, v in time_localization.items():
        if v[1] != 1:
            v[0] = 1
        out.write(str(k) + ': ' + str(v[0]) + '\n')
    out.close()
    
    out = open(path_to_metrics + '/data_alignment_metric', 'w')
    out.write('Выравнивание данных: \n')
    out.write('Количество обращений к выравненным адресам: ' + str(aligned_addresses) + '\n')
    out.write('Количество обращений к невыравненным адресам: ' + str(unaligned_addresses) + '\n')
    out.write('Отношение количества обращений к выраненным адресам к количеству обращений к невыравненным адресам: ' + str(aligned_addresses/unaligned_addresses))
    out.close()
    
    out = open(path_to_metrics + '/space_localization_metric', 'w')
    out.write('Пространственная локализация данных: суммарное кол-во обращений по одному блоку (32 байта)/ Количество команд чтения и записи\n')
    out.write('Номер блока: Результат\n')
    for k, v in sorted(clasters.items()):
        out.write(str(k) + ': ' + str(v/ldst_cnt) + '\n')
    out.close()