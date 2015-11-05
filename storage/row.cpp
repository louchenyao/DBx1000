#include "global.h"
#include "table.h"
#include "catalog.h"
#include "row.h"
#include "txn.h"
#include "row_lock.h"
#include "row_ts.h"
#include "row_mvcc.h"
#include "row_occ.h"
#include "row_specex.h"
#include "row_vll.h"
#include "mem_alloc.h"
#include "manager.h"

RC 
row_t::init(table_t * host_table, uint64_t part_id, uint64_t row_id) {
	part_info = true;
	_row_id = row_id;
	_part_id = part_id;
	this->table = host_table;
	Catalog * schema = host_table->get_schema();
	tuple_size = schema->get_tuple_size();
	//data = (char *) mem_allocator.alloc(sizeof(char) * tuple_size, _part_id);
	data = (char *) mem_allocator.alloc(sizeof(char) * 1, _part_id);
	return RCOK;
}

RC 
row_t::switch_schema(table_t * host_table) {
	this->table = host_table;
	return RCOK;
}

void row_t::init_manager(row_t * row) {
#if MODE==NOCC_MODE || MODE==QRY_ONLY_MODE
  return;
#endif
  DEBUG_M("row_t::init_manager alloc \n");
#if CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE || CC_ALG == CALVIN
    manager = (Row_lock *) mem_allocator.alloc(sizeof(Row_lock), _part_id);
#elif CC_ALG == TIMESTAMP
    manager = (Row_ts *) mem_allocator.alloc(sizeof(Row_ts), _part_id);
#elif CC_ALG == MVCC
    manager = (Row_mvcc *) mem_allocator.alloc(sizeof(Row_mvcc), _part_id);
#elif CC_ALG == OCC
    manager = (Row_occ *) mem_allocator.alloc(sizeof(Row_occ), _part_id);
#elif CC_ALG == VLL
    manager = (Row_vll *) mem_allocator.alloc(sizeof(Row_vll), _part_id);
    /*
#elif CC_ALG == HSTORE_SPEC
    manager = (Row_specex *) mem_allocator.alloc(sizeof(Row_specex), _part_id);
    */
#endif

#if CC_ALG != HSTORE && CC_ALG != HSTORE_SPEC 
	manager->init(this);
#endif
}

table_t * row_t::get_table() { 
	return table; 
}

Catalog * row_t::get_schema() { 
	return get_table()->get_schema(); 
}

const char * row_t::get_table_name() { 
	return get_table()->get_table_name(); 
};
uint64_t row_t::get_tuple_size() {
	return get_schema()->get_tuple_size();
}

uint64_t row_t::get_field_cnt() { 
	return get_schema()->field_cnt;
}

void row_t::set_value(int id, void * ptr) {
	int datasize = get_schema()->get_field_size(id);
	int pos = get_schema()->get_field_index(id);
  char d[tuple_size];
	memcpy( &d[pos], ptr, datasize);
	//memcpy( &data[pos], ptr, datasize);
}

void row_t::set_value(int id, void * ptr, int size) {
	int pos = get_schema()->get_field_index(id);
  char d[tuple_size];
	memcpy( &d[pos], ptr, size);
	//memcpy( &data[pos], ptr, size);
}

void row_t::set_value(const char * col_name, void * ptr) {
	uint64_t id = get_schema()->get_field_id(col_name);
	set_value(id, ptr);
}

SET_VALUE(uint64_t);
SET_VALUE(int64_t);
SET_VALUE(double);
SET_VALUE(UInt32);
SET_VALUE(SInt32);

GET_VALUE(uint64_t);
GET_VALUE(int64_t);
GET_VALUE(double);
GET_VALUE(UInt32);
GET_VALUE(SInt32);

char * row_t::get_value(int id) {
  int pos __attribute__ ((unused));
	pos = get_schema()->get_field_index(id);
	return data;
	//return &data[pos];
}

char * row_t::get_value(char * col_name) {
  uint64_t pos __attribute__ ((unused));
	pos = get_schema()->get_field_index(col_name);
	return data;
	//return &data[pos];
}

char * row_t::get_data() { return data; }
void row_t::set_data(char * data) { 
	int tuple_size = get_schema()->get_tuple_size();
  char d[tuple_size];
	memcpy(d, data, tuple_size);
	//memcpy(this->data, data, tuple_size);
}
// copy from the src to this
void row_t::copy(row_t * src) {
	assert(src->get_schema() == this->get_schema());
	set_data(src->get_data());
}

void row_t::free_row() {
  DEBUG_M("row_t::free_row free\n");
	//mem_allocator.free(data, sizeof(char) * get_tuple_size());
	mem_allocator.free(data, sizeof(char) * 1);
}

RC row_t::get_lock(access_t type, txn_man * txn) {
  RC rc = RCOK;
#if CC_ALG == CALVIN
	lock_t lt = (type == RD || type == SCAN)? LOCK_SH : LOCK_EX;
	rc = this->manager->lock_get(lt, txn);
#endif
  return rc;
}

RC row_t::get_row(access_t type, txn_man * txn, row_t *& row) {
	RC rc = RCOK;
#if MODE==NOCC_MODE || MODE==QRY_ONLY_MODE 
  txn->rc = rc;
  row = this;
  return rc;
#endif
  uint64_t thd_prof_start = get_sys_clock();
#if CC_ALG == WAIT_DIE || CC_ALG == NO_WAIT || CC_ALG == DL_DETECT 
	//uint64_t thd_id = txn->get_thd_id();
	lock_t lt = (type == RD || type == SCAN)? LOCK_SH : LOCK_EX;
#if CC_ALG == DL_DETECT
	uint64_t * txnids;
	int txncnt; 
	rc = this->manager->lock_get(lt, txn, txnids, txncnt);	
#else
	rc = this->manager->lock_get(lt, txn);
#endif

	if (rc == RCOK) {
    txn->rc = rc; 
		row = this;
	} else if (rc == Abort) {} 
	else if (rc == WAIT) {
		ASSERT(CC_ALG == WAIT_DIE || CC_ALG == DL_DETECT);
#if CC_ALG == DL_DETECT	
		bool dep_added = false;
#endif
    // lock_abort only used by DL_DETECT
		//txn->lock_abort = false;
		INC_STATS(0, wait_cnt, 1);

	}
	goto end;
#elif CC_ALG == TIMESTAMP || CC_ALG == MVCC 
	//uint64_t thd_id = txn->get_thd_id();
	// For TIMESTAMP RD, a new copy of the row will be returned.
	// for MVCC RD, the version will be returned instead of a copy
	// So for MVCC RD-WR, the version should be explicitly copied.
	// row_t * newr = NULL;
#if CC_ALG == TIMESTAMP
	// TIMESTAMP makes a whole copy of the row before reading
  DEBUG_M("row_t::get_row TIMESTAMP alloc \n");
	txn->cur_row = (row_t *) mem_allocator.alloc(sizeof(row_t), this->get_part_id());
	txn->cur_row->init(get_table(), this->get_part_id());
#endif
	
	if (type == WR) {
		rc = this->manager->access(txn, P_REQ, NULL);
		if (rc != RCOK) 
			goto end;
	}
	if ((type == WR && rc == RCOK) || type == RD || type == SCAN) {
		rc = this->manager->access(txn, R_REQ, NULL);
		if (rc == RCOK ) {
			row = txn->cur_row;
		} else if (rc == WAIT) {
			//uint64_t t1 = get_sys_clock();
      // TODO: divide into 2+ functions to restart after ts_ready 
			//while (!txn->ts_ready) {}
		  INC_STATS(0, wait_cnt, 1);
      rc = WAIT;
      goto end;

      /*
			uint64_t t2 = get_sys_clock();
			INC_STATS(thd_id, time_wait, t2 - t1);
			row = txn->cur_row;
      */
		} else if (rc == Abort) { }
		if (rc != Abort) {
			assert(row->get_data() != NULL);
			assert(row->get_table() != NULL);
			assert(row->get_schema() == this->get_schema());
			assert(row->get_table_name() != NULL);
		}
	}
	if (rc != Abort && CC_ALG == MVCC && type == WR) {
    DEBUG_M("row_t::get_row MVCC alloc \n");
		row_t * newr = (row_t *) mem_allocator.alloc(sizeof(row_t), get_part_id());
		newr->init(this->get_table(), get_part_id());
		newr->copy(row);
		row = newr;
	}
	goto end;
#elif CC_ALG == OCC
	// OCC always make a local copy regardless of read or write
  DEBUG_M("row_t::get_row OCC alloc \n");
	txn->cur_row = (row_t *) mem_allocator.alloc(sizeof(row_t), get_part_id());
	txn->cur_row->init(get_table(), get_part_id());
	rc = this->manager->access(txn, R_REQ);
	row = txn->cur_row;
	goto end;
#elif CC_ALG == HSTORE || CC_ALG == HSTORE_SPEC || CC_ALG == VLL || CC_ALG == CALVIN
#if CC_ALG == HSTORE_SPEC
  if(txn_table.spec_mode) {
    DEBUG_M("row_t::get_row HSTORE_SPEC alloc \n");
	  txn->cur_row = (row_t *) mem_allocator.alloc(sizeof(row_t), get_part_id());
	  txn->cur_row->init(get_table(), get_part_id());
	  rc = this->manager->access(txn, R_REQ);
	  row = txn->cur_row;
	  goto end;
  }
#endif
	row = this;
	goto end;
#else
	assert(false);
#endif

end:
  INC_STATS(txn->get_thd_id(),thd_prof_row1,get_sys_clock() - thd_prof_start);
  return rc;
}

// Return call for get_row if waiting 
RC row_t::get_row_post_wait(access_t type, txn_man * txn, row_t *& row) {

  uint64_t thd_prof_start = get_sys_clock();
  RC rc = RCOK;
  assert(CC_ALG == WAIT_DIE || CC_ALG == MVCC || CC_ALG == TIMESTAMP);
#if CC_ALG == WAIT_DIE
  assert(txn->lock_ready);
	rc = RCOK;
	//ts_t endtime = get_sys_clock();
	//INC_STATS(thd_id, time_wait, endtime - starttime);
	row = this;

#elif CC_ALG == MVCC || CC_ALG == TIMESTAMP
			assert(txn->ts_ready);
			//INC_STATS(thd_id, time_wait, t2 - t1);
			row = txn->cur_row;

			assert(row->get_data() != NULL);
			assert(row->get_table() != NULL);
			assert(row->get_schema() == this->get_schema());
			assert(row->get_table_name() != NULL);
	if (CC_ALG == MVCC && type == WR) {
    DEBUG_M("row_t::get_row_post_wait MVCC alloc \n");
		row_t * newr = (row_t *) mem_allocator.alloc(sizeof(row_t), get_part_id());
		newr->init(this->get_table(), get_part_id());

		newr->copy(row);
		row = newr;
	}
#endif
  INC_STATS(txn->get_thd_id(),thd_prof_row2,get_sys_clock() - thd_prof_start);
  return rc;
}

// the "row" is the row read out in get_row(). For locking based CC_ALG, 
// the "row" is the same as "this". For timestamp based CC_ALG, the 
// "row" != "this", and the "row" must be freed.
// For MVCC, the row will simply serve as a version. The version will be 
// delete during history cleanup.
// For TIMESTAMP, the row will be explicity deleted at the end of access().
// (c.f. row_ts.cpp)
void row_t::return_row(access_t type, txn_man * txn, row_t * row) {	
#if MODE==NOCC_MODE || MODE==QRY_ONLY_MODE
  return;
#endif
  uint64_t thd_prof_start = get_sys_clock();
#if CC_ALG == WAIT_DIE || CC_ALG == NO_WAIT || CC_ALG == DL_DETECT || CC_ALG == CALVIN
	assert (row == NULL || row == this || type == XP);
	if (ROLL_BACK && type == XP) {// recover from previous writes.
		this->copy(row);
	}
	this->manager->lock_release(txn);
#elif CC_ALG == TIMESTAMP || CC_ALG == MVCC 
	// for RD or SCAN or XP, the row should be deleted.
	// because all WR should be companied by a RD
	// for MVCC RD, the row is not copied, so no need to free. 
	if (CC_ALG == TIMESTAMP && (type == RD || type == SCAN)) {
		row->free_row();
    DEBUG_M("row_t::return_row TIMESTAMP free \n");
		mem_allocator.free(row, sizeof(row_t));
	}
	if (type == XP) {
		row->free_row();
    DEBUG_M("row_t::return_row XP free \n");
		mem_allocator.free(row, sizeof(row_t));
		this->manager->access(txn, XP_REQ, NULL);
	} else if (type == WR) {
		assert (type == WR && row != NULL);
		assert (row->get_schema() == this->get_schema());
		RC rc = this->manager->access(txn, W_REQ, row);
		assert(rc == RCOK);
	}
#elif CC_ALG == OCC
	assert (row != NULL);
	if (type == WR)
		manager->write( row, txn->end_ts );
	row->free_row();
  DEBUG_M("row_t::return_row OCC free \n");
	mem_allocator.free(row, sizeof(row_t));
  manager->release();
	return;
  // FIXME: VLL?
#elif CC_ALG == HSTORE || CC_ALG == HSTORE_SPEC || CC_ALG == VLL
	assert (row != NULL);
	if (ROLL_BACK && type == XP) {// recover from previous writes.
		this->copy(row);
	}
	return;
#else 
	assert(false);
#endif
  INC_STATS(txn->get_thd_id(),thd_prof_row3,get_sys_clock() - thd_prof_start);
}

