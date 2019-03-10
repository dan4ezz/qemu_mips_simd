def create_new_log(path1, path2):
    file = open(f'{path1}/log_msa')
    log_msa = list(file)[1:]
    file.close()
    file = open(f'{path2}/log_msa')
    log_main = list(file)[1:]
    file.close()

    cnt_from_start = 0
    cnt_from_end = 0

    for line_msa, line_main in zip(log_msa, log_main):
        if line_msa[:line_msa.find('0')] == line_main[:line_main.find('0')]:
            cnt_from_start += 1
        else:
            break

    for line_msa, line_main in zip(reversed(log_msa), reversed(log_main)):
        if line_msa[:line_msa.find('0')] == line_main[:line_main.find('0')]:
            cnt_from_end += 1
        else:
            break

    log_msa_reduced = log_msa[cnt_from_start:-cnt_from_end]
    del log_msa
    del log_main

    print(cnt_from_start, cnt_from_end)
    # if not log_main_reduced:
    print('not')
    with open(f'{path1}/new_log', 'w') as log:
        for item in log_msa_reduced:
            log.write(item)

    # Непонятно как убирать команды, которые вызываются не для алгоритма, если не весь алгоритм находится в
    # main-е, так как у команд меняются адреса, также меняются и сами команды

    # offset = 10
    # else:
    #     log_main_commands = []
    #     log_msa_commands = []
    #     for line_main, line_msa in zip(log_main_reduced, log_msa_reduced):
    #         log_main_commands.append(line_main[:line_main.find('0')])
    #         log_msa_commands.append(line_msa[:line_msa.find('0')])
    #     i = 0
    #     list_of_indx = []
    #     for indx in range(len(log_msa_commands)):
    #         if log_msa_commands[indx + offset*i:offset*(i+1) + indx] == log_main_commands[offset*i + 300: offset*(i+1)+300]:
    #             list_of_indx.append(indx)
    #             i += 1
    #             if len(log_main_commands) - offset*i <= 0:
    #                 break