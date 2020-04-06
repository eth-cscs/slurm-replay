from sqlalchemy import create_engine
import pymysql
import pandas as pd
import numpy as np
import time
from datetime import datetime
import ctypes

MAX_CHAR='S1024'
TINYTEXT_SIZE='S128'
TEXT_SIZE='S8192'
LARGETEXT_SIZE='S16384'
job_trace_t = np.dtype([
    ("account", TINYTEXT_SIZE),
    ("exit_code", 'i4'),
    ("job_name", TINYTEXT_SIZE),
    ("id_job", 'i4'),
    ("qos_name", TINYTEXT_SIZE),
    ("id_user", 'i4'),
    ("id_group", 'i4'),
    ("resv_name", TINYTEXT_SIZE),
    ("nodelist", LARGETEXT_SIZE),
    ("nodes_alloc", 'i4'),
    ("partition", TINYTEXT_SIZE),
    ("dependencies", TEXT_SIZE),
    ("switches", 'i4'),
    ("state", 'i4'),
    ("timelimit", 'i4'),
    ("time_submit", 'i8'),
    ("time_eligible", 'i8'),
    ("time_start", 'i8'),
    ("time_end", 'i8'),
    ("time_suspended", 'i8'),
    ("gres_alloc", TEXT_SIZE),
    ("preset", 'i4'),
    ("priority", 'i4'),
    ("user", TINYTEXT_SIZE),
    ])

def find_node_type(gres):
    if gres.startswith('craynetwork:') or \
       gres.startswith('gpu:0') or \
       gres.startswith('7696487:0'):
        return 'mc'
    elif gres.startswith('gpu:') or gres.startswith('7696487:'):
        return 'gpu'
    return gres

def state_str(state):
    switcher = { 0: 'PD', 1: 'RU', 2: 'SU', 3: 'CP', 4: 'CA', 5: 'FA', 6: 'TO', 10: 'DL', 11:'OOM'}
    if state in switcher.keys():
        return switcher[state]
    else:
        return str(state)

def getworkload_binarytodf(wl_name):
    f = open(wl_name, "rb")
    query_length = np.fromfile(f, dtype=np.int64, count=1)
    query_t = np.dtype((np.str_, query_length[0]))
    query = np.fromfile(f, dtype='S1', count=query_length[0]).tostring()
    print(query)
    num_rows =  np.fromfile(f, dtype=np.int64, count=1)
    print("Number of rows:",num_rows[0])
    data = np.fromfile(f, dtype=job_trace_t, count=num_rows[0])
    df = pd.DataFrame(data, columns=job_trace_t.names)
    str_df = df.select_dtypes([np.object])
    str_df = str_df.stack().str.decode('ascii').unstack()
    for col in str_df:
        df[col] = str_df[col]
    return df

# to make it nicer with more argurments
def getworkload_mysqltodf(start_date, end_date, extra_where):
    query_workload="""
    SELECT t.account, t.exit_code, t.job_name, t.id_job,
           q.name, t.id_user, t.id_group, r.resv_name,
           t.nodelist, t.nodes_alloc, t.partition,
           t.state, t.timelimit, t.time_submit,
           t.time_eligible, t.time_start, t.time_end,
           t.time_suspended, t.gres_alloc, t.priority, a.user, c.organization
           FROM daint_job_table as t
                LEFT JOIN daint_resv_table as r ON t.id_resv = r.id_resv
                                                AND t.time_start >= r.time_start
                                                AND t.time_end <= r.time_end
                LEFT JOIN qos_table as q ON q.id = t.id_qos
                LEFT JOIN daint_assoc_table as a ON t.id_assoc = a.id_assoc
                LEFT JOIN acct_table as c ON a.acct = c.name
           WHERE    t.time_submit <= %d
                AND t.time_end >= %d
                AND t.time_start >= %d AND t.time_start <= %d
                AND t.state <> 7 AND t.state < 12
                AND t.nodes_alloc > 0
                AND r.resv_name IS NULL
                AND t.partition IN ('normal')
    """ % (int(datetime.timestamp(end_date)), int(datetime.timestamp(start_date)),
           int(datetime.timestamp(start_date)),int(datetime.timestamp(end_date)))
    #AND t.time_start < %d
    #int(datetime.timestamp(end_date))
    if extra_where and len (extra_where) > 0:
        query_workload += " AND "+extra_where
    print(query_workload)

    db_connection_str='mysql+pymysql://read:readonly@slurmdb.cscs.ch:3307/slurmdbd_prod'
    db_connection = create_engine(db_connection_str)
    df = pd.read_sql(query_workload, con=db_connection)

    print("Number of tuples: %d" % (len(df)))
    return df

def preprocessed_wordload(df):
    start_date = df['time_submit'].min()
    df['new_time_submit'] = df['time_submit']
    df.loc[df['new_time_submit'] < start_date, 'new_time_submit'] = start_date

    df['time_wait'] = df['time_start'] - df['time_submit']
    df['time_elapsed'] = df['time_end'] - df['time_start']
    df['nodehours'] = (df['time_end'] - df['time_start']) * df['nodes_alloc']
    df['node_type'] = df['gres_alloc'].map(lambda x: find_node_type(x))
    print(df['node_type'].unique())
    print(df.groupby('node_type')['node_type'].count())

    #if 'time_submit' in df:
    #    df['time_submit'] = df['time_submit'].apply(lambda x: x if x > int(datetime.timestamp(start_date)) else int(datetime.timestamp(start_date)))
    #    df['time_submit'] = pd.to_datetime(df['time_submit'], unit='s')
    #    df = df.set_index('time_submit')
    #df['time_start'] = pd.to_datetime(df['time_start'], unit='s')
    #df['time_end'] = pd.to_datetime(df['time_end'], unit='s')

    df['state'] = df['state'].apply(lambda x: state_str(x))
    return df

def analyse_workload(df, end_date):
    start_date = df['time_submit'].min()
    print("njobs:",len(df))
    print("accounts:",sorted(df['account'].unique()))
    print("nb accounts:",len(df['account'].unique()))
    #print("reservation:",df['resv_name'].unique())
    #print("partition:",df['partition'].unique())
    #print("qos name:",df['name'].unique())
    total_nodehours = df['nodehours'].agg('sum')
    total_nodes_alloc = df['nodes_alloc'].agg('sum')
    print("total_nodehours ", total_nodehours, " total_nodes_alloc ", total_nodes_alloc)
    print("max node:",df['nodes_alloc'].max(),"| min node:",df['nodes_alloc'].min())
    print("states:", df.groupby('state')['state'].count())
    print(df.groupby('nodes_alloc')['nodes_alloc'].count())
    print(df[df.groupby('account')['account'].transform('size') > 1000].groupby('account')['account'].count().reset_index(name='count').sort_values(['count'], ascending=False))
    print(df[df.groupby('user')['user'].transform('size') > 100].groupby('user')['user'].count().reset_index(name='count').sort_values(['count'], ascending=False))
    agg_df = df.groupby('user').agg({'nodehours':'sum', 'nodes_alloc': 'sum', 'user': 'count'}).sort_values(['nodehours'], ascending=False)
    agg_df['per_nodehours'] = agg_df['nodehours']/total_nodehours*100
    agg_df['per_nodes_alloc'] = agg_df['nodes_alloc']/total_nodes_alloc*100
    print(agg_df)
    bins = [ k for k in range(start_date, end_date, 4*3600)]
    print([str(k)+'='+time.ctime(k) for k in bins])
    print(df.groupby(pd.cut(df['time_submit'], bins=bins)).size())
    print(df.groupby(pd.cut(df['time_submit'], bins=bins))['nodehours'].sum())

    new_df = df[['nodes_alloc', 'time_submit', 'user', 'state', 'nodehours']]
    new_df['time_submit'] = pd.to_datetime(new_df['time_submit'], unit='s')
    print(new_df[new_df['nodes_alloc'] > 1000])
    print(df['time_wait'].describe())
    print(df['time_elapsed'].describe())
    print(df['nodes_alloc'].describe())

