import json
import collections as col
from log_sub import create_new_log
import os
import sys


def load_config():
    config = {
        "path_to_log": sys.argv[1],
        "path_to_log_with_main": sys.argv[2],
        "path_to_metrics_folder": sys.argv[3]
    }
    return config


def create_metrics(path_to_log, path_to_metrics):
    ldst_list = ['LB', 'LBU', 'LWU', 'LD', 'LLD', 'LDL', 'LDR', 'LDPC', 'LWPC', 'LWE', 'LW', 'LHE', 'LH', 'LHUE',
                 'LHU', 'LBE', 'LBUE', 'LWLE', 'LWL', 'LWRE', 'LWR', 'LLE', 'LL', 'SD', 'SDL', 'SDR', 'SWE', 'SW',
                 'SHE', 'SH', 'SBE', 'SB', 'SWLE', 'SWL', 'SWRE', 'SWR', 'SCD', 'SCE', 'SC', 'LWC1', 'SWC1', 'LDC1',
                 'SDC1', 'MSA_LD_B', 'MSA_LD_H', 'MSA_LD_W', 'MSA_LD_D', 'MSA_ST_B', 'MSA_ST_H', 'MSA_ST_W',
                 'MSA_ST_D', 'MSA_LDI_df']

    ldst_cnt = 0
    aligned_addresses = 0
    unaligned_addresses = 0

    clasters = col.defaultdict(int)
    time_localization = col.defaultdict(list)
    time_loc_scnd = col.defaultdict(list)

    msa_cnt = 0
    msa_ldst_cnt = 0

    with open(f'{path_to_log}/new_log', 'r') as log_file:
        for index, line in enumerate(log_file):
            command = line[:line.find(" ")]
            indx = line.find("0")
            c_address = line[indx:-1]
            if command in ldst_list:
                ldst_cnt += 1

            # Оценка векторизации
            if 'MSA' in command and command in ldst_list:
                msa_ldst_cnt += 1
            if 'MSA' in command and command not in ldst_list:
                msa_cnt += 1

            # If command has address
            if indx != -1:
                # Выравнивание данных
                if int(c_address, 16) % 16 == 0:
                    aligned_addresses += 1
                else:
                    unaligned_addresses += 1

                # Пространственная локализация
                claster = int(c_address, 16) // 32
                # Если кластера еще нет в ключах, то берется значение, возвращаемое int() - 0
                clasters[claster] += 1

                # Временная локализация - 1 вариант, считается количество всех адресов между двумя обращениями

                # Если адрес встречается первый раз
                if c_address not in time_localization.keys():

                    # dict: {address:[cnt, flag]}, cnt - количество адресов, flag = 0 => увеличиваем cnt
                    # flag = 1 означает, что для данного адреса уже все посчитано
                    time_localization[c_address].append(0)
                    time_localization[c_address].append(0)

                    # Проходим по всем адресам
                    for k, v in time_localization.items():
                        if time_localization[k][1] == 0:
                            v[0] += 1
                else:
                    time_localization[c_address][1] = 1
                    for k, v in time_localization.items():
                        if time_localization[k][1] == 0:
                            v[0] += 1

                # Временная локализация - 2 вариант, считается количество уникальных адресов между двумя обращениями

                if c_address not in time_loc_scnd.keys():
                    time_loc_scnd[c_address].append(0)
                    time_loc_scnd[c_address].append(0)
                    # Индекс - индикатор, который позволяет определять уникальность адреса для оперделенного интервала
                    time_loc_scnd[c_address].append(index)
                    for k, v in time_loc_scnd.items():
                        if time_loc_scnd[k][1] == 0:
                            v[0] += 1
                else:
                    time_loc_scnd[c_address][1] = 1
                    for k, v in time_loc_scnd.items():
                        if time_loc_scnd[k][2] > time_loc_scnd[c_address][2] and time_loc_scnd[k][1] == 0:
                            v[0] += 1
                            time_loc_scnd[c_address][2] = index

    # Создание папки с метриками и их вывод
    if not os.path.exists(f'{path_to_metrics}/metrics'):
        os.mkdir(f'{path_to_metrics}/metrics')

    with open(f'{path_to_metrics}/metrics/vectorization', 'w') as file:
        try:
            msa_cnt / msa_ldst_cnt
        except ZeroDivisionError as e:
            with open(f'{path_to_metrics}/errors', 'w') as error_file:
                error_file.write(str(e)+' in vectorization\n')
        else:
            file.write(str(msa_cnt/msa_ldst_cnt))

    with open(f'{path_to_metrics}/metrics/space_localization', 'w') as file:
        for k, v in sorted(clasters.items()):
            file.write(str(k) + ': ' + str(v / ldst_cnt) + '\n')

    with open(f'{path_to_metrics}/metrics/data_alignment', 'w') as file:
        file.write(str(aligned_addresses) + '\n')
        file.write(str(unaligned_addresses) + '\n')
        try:
            res = aligned_addresses / unaligned_addresses
        except ZeroDivisionError as e:
            with open(f'{path_to_metrics}/errors', 'w') as error_file:
                error_file.write(str(e)+' in data alignment\n')
        else:
            file.write(str(res))

    with open(f'{path_to_metrics}/metrics/time_localization_1st', 'w') as file:
        for k, v in time_localization.items():
            if v[1] != 1:
                v[0] = 1
            file.write(str(k) + ': ' + str(v[0]) + '\n')

    with open(f'{path_to_metrics}/metrics/time_localization_2nd', 'w') as file:
        for k, v in time_loc_scnd.items():
            if v[1] != 1:
                v[0] = 1
            file.write(str(k) + ': ' + str(v[0]) + '\n')


def main():
    config = load_config()
    create_new_log(config['path_to_log'], config['path_to_log_with_main'])
    create_metrics(config['path_to_log'], config['path_to_metrics_folder'])


if __name__ == '__main__':
    main()


