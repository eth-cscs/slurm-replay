import pandas as pd
import time
from datetime import datetime
from workload import getworkload_binarytodf, analyse_workload, preprocessed_wordload
import pickle
import os
import argparse

parser = argparse.ArgumentParser(description='Create a pickle of a df reprensenting a workload generated by trace_builder_mysql')
parser.add_argument('-wl', type=str, default=None, help='workload file')

args = parser.parse_args()
wl_name = args.wl

df = getworkload_binarytodf(wl_name)
df = preprocessed_wordload(df)
df.to_pickle(wl_name+'.pickle')

analyse_workload(df, 1552172399)
