#include "global.h"
#include "mem_alloc.h"
#include "stats.h"
#include "dl_detect.h"
#include "manager.h"
#include "query.h"
#include "plock.h"
#include "occ.h"
#include "vll.h"
#include "log.h"
#include "serial_log.h"
#include "parallel_log.h"
#include "log_pending_table.h"
#include "log_recover_table.h"
#include "free_queue.h"

mem_alloc mem_allocator;
Stats * stats;
#if CC_ALG == DL_DETECT
DL_detect dl_detector;
#endif
Manager * glob_manager;
Query_queue * query_queue;
Plock part_lock_man;
OptCC occ_man;

// Logging
//#if LOG_ALGORITHM == LOG_SERIAL
//boost::lockfree::spsc_queue<RecoverState *, boost::lockfree::capacity<1000>> ** txns_ready_for_recovery;
//RSQueue ** rs_queue; 
//#else
//boost::lockfree::queue<RecoverState *> ** txns_ready_for_recovery;
//#endif
#if LOG_ALGORITHM == LOG_SERIAL
LogManager * log_manager;
#elif LOG_ALGORITHM == LOG_BATCH
LogManager ** log_manager;
#elif LOG_ALGORITHM == LOG_PARALLEL
LogManager ** log_manager; 
LogRecoverTable * log_recover_table;
uint64_t * starting_lsn;
#endif
uint32_t g_epoch_period = EPOCH_PERIOD;
uint32_t ** next_log_file_epoch;

FreeQueue ** free_queue_recover_state; 
bool g_log_recover = LOG_RECOVER;
uint32_t g_num_logger = NUM_LOGGER;
bool g_no_flush = LOG_NO_FLUSH;


#if CC_ALG == VLL
VLLMan vll_man;
#endif 

bool volatile warmup_finish = false;
bool volatile enable_thread_mem_pool = false;
pthread_barrier_t warmup_bar;
pthread_barrier_t log_bar;
pthread_barrier_t worker_bar;
#ifndef NOGRAPHITE
carbon_barrier_t enable_barrier;
#endif

ts_t g_abort_penalty = ABORT_PENALTY;
bool g_central_man = CENTRAL_MAN;
UInt32 g_ts_alloc = TS_ALLOC;
bool g_key_order = KEY_ORDER;
bool g_no_dl = NO_DL;
ts_t g_timeout = TIMEOUT;
ts_t g_dl_loop_detect = DL_LOOP_DETECT;
bool g_ts_batch_alloc = TS_BATCH_ALLOC;
UInt32 g_ts_batch_num = TS_BATCH_NUM;

bool g_part_alloc = PART_ALLOC;
bool g_mem_pad = MEM_PAD;
UInt32 g_cc_alg = CC_ALG;
ts_t g_query_intvl = QUERY_INTVL;
UInt32 g_part_per_txn = PART_PER_TXN;
double g_perc_multi_part = PERC_MULTI_PART;

double g_read_perc = READ_PERC;
double g_zipf_theta = ZIPF_THETA;
bool g_prt_lat_distr = PRT_LAT_DISTR;
UInt32 g_part_cnt = PART_CNT;
UInt32 g_virtual_part_cnt = VIRTUAL_PART_CNT;
UInt32 g_thread_cnt = THREAD_CNT;
UInt64 g_synth_table_size = SYNTH_TABLE_SIZE;
UInt32 g_req_per_query = REQ_PER_QUERY;
UInt32 g_field_per_tuple = FIELD_PER_TUPLE;
UInt32 g_init_parallelism = INIT_PARALLELISM;
uint64_t g_max_txns_per_thread = MAX_TXNS_PER_THREAD;

UInt32 g_num_wh = NUM_WH;
double g_perc_payment = PERC_PAYMENT;
bool g_wh_update = WH_UPDATE;
char * output_file = NULL;
char * logging_dir = NULL;
uint32_t g_log_parallel_num_buckets = LOG_PARALLEL_NUM_BUCKETS;

UInt32 g_log_buffer_size = LOG_BUFFER_SIZE;

//map<string, string> g_params;
bool g_abort_buffer_enable = ABORT_BUFFER_ENABLE;
bool g_pre_abort = PRE_ABORT;
bool g_atomic_timestamp = ATOMIC_TIMESTAMP;
string g_write_copy_form = WRITE_COPY_FORM;
string g_validation_lock = VALIDATION_LOCK;

#if TPCC_SMALL
UInt32 g_max_items = 10000;
UInt32 g_cust_per_dist = 2000;
#else 
UInt32 g_max_items = 100000;
UInt32 g_cust_per_dist = 3000;
#endif
