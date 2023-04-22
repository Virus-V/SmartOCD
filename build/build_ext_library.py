#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import subprocess
import argparse
import shlex
from tempfile import NamedTemporaryFile
from shutil import copyfile
from datetime import datetime


def cmd_exec(command, temp_file, error_log_path):
    start_time = datetime.now().replace(microsecond=0)
    cmd = shlex.split(command)

    proc = subprocess.Popen(cmd,
                            stdout=temp_file,
                            stderr=temp_file,
                            encoding='utf-8')

    proc.wait()
    ret_code = proc.returncode
    if ret_code != 0:
        temp_file.close()
        copyfile(temp_file.name, error_log_path)
        os.remove(temp_file.name)
        return ret_code

    end_time = datetime.now().replace(microsecond=0)
    temp_file.write(f'cmd:{command}\ncost time:{end_time-start_time}\n')
    return ret_code


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--path', help='Build path.')
    parser.add_argument('--prebuilts', help='Build prebuilts.')
    parser.add_argument('--command', help='Build command.')
    parser.add_argument('--enable', help='enable python.', nargs='*')
    parser.add_argument('--target_dir', nargs=1)
    parser.add_argument('--out_dir', nargs=1)
    args = parser.parse_args()

    if args.enable:
        if args.enable[0] == 'false':
            return

    if args.path:
        curr_dir = os.getcwd()
        os.chdir(args.path)
        temp_file = NamedTemporaryFile(mode='wt', delete=False)
        if args.prebuilts:
            status = cmd_exec(args.prebuilts, temp_file, args.out_dir[0])
            if status != 0:
                return status
        if args.command:
            if '&&' in args.command:
                command = args.command.split('&&')
                for data in command:
                    status = cmd_exec(data, temp_file, args.out_dir[0])
                    if status != 0:
                        return status
            else:
                status = cmd_exec(args.command, temp_file, args.out_dir[0])
                if status != 0:
                    return status
        temp_file.close()
        copyfile(temp_file.name, args.target_dir[0])
        os.remove(temp_file.name)

        os.chdir(curr_dir)
    return 0


if __name__ == '__main__':
    sys.exit(main())
