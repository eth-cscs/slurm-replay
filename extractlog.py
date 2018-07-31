from elasticsearch import Elasticsearch
from elasticsearch import helpers
import re


# The log should contained the script submitted for every job
# from that script, `--dependency` and `--switch` options are parsed

# Location of the elastic search service
es_location = 'http://sole01:9200/'

# range of dates for slurm jobs
start_date="2018-01-01"
end_date="2018-06-30"

# cluster name
cluster='daint'

# search index
index='slurm*'


def getOptionFromScript(script, option):
    r = re.compile('--'+option+'=[^\n ]*')
    for l in script.split('\n'):
        if l.startswith('#SBATCH') or not l.startswith('#'):
            k = r.findall(l)
            if k:
                return k[0]

if __name__ == '__main__':
    es = Elasticsearch([es_location])
    # get list of jobs
    slurmdb = es.search(index=index, scroll='2h', size = 1000, sort='jobid',
            body={"query":
                  {"bool":
                   { "must": [
                      {"query_string": { "query": "cluster: "+cluster+" AND _exists_:script", "allow_leading_wildcard": False }},
                      {"range": {"@submit": {"gte": start_date, "lte": end_date}}}
                   ] }
                   }})
    sid = slurmdb['_scroll_id']
    scroll_size = slurmdb['hits']['total']

    while (scroll_size > 0):
        slurmdb = es.scroll(scroll_id = sid, scroll = '2h')
        sid = slurmdb['_scroll_id']
        scroll_size = len(slurmdb['hits']['hits'])
        # for each jobs retrieve logs and filter them
        for h in slurmdb['hits']['hits']:
            switches = None
            s = getOptionFromScript(h['_source']['script'], "switches")
            if s:
                switches = s.replace('--switches=','')
            dependencies = None
            if 'orig_dependency' in h['_source']:
                dependencies = h['_source']['orig_dependency']
            else:
                d = getOptionFromScript(h['_source']['script'], "dependency")
                if d:
                   dependencies = d.replace('--dependency=','').replace('$SLURM_JOB_ID',str(h['_source']['jobid']))
            if dependencies or switches:
                if not dependencies:
                    dependencies=''
                if not switches:
                    switches=''
                print("%d|%s|%s|" % (h['_source']['jobid'], dependencies, switches))
