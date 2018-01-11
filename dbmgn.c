#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "acarsserv.h"

static sqlite3 *acarsdb;

#define NBTRANS 10
static sqlite3_stmt *stm[NBTRANS];

#define TBT 0
#define TET 1
#define TRL 2
#define TSELFLG 3
#define TINSFLG 4
#define TUPFLG 5
#define TSELMSG 6
#define TINSMSG 7
#define TSELST 8
#define TINSST 9

int initdb(char *dbname)
{
	int res;
	int i;
	const char *sql[NBTRANS];

	sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

	res = sqlite3_open(dbname, &acarsdb);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to open database %s\n", dbname);
		return 1;
	}

	sql[TBT] = "begin transaction";
	sql[TET] = "end transaction";
	sql[TRL] = "rollback transaction";
	sql[TSELFLG] =
	    "select FlightID from Flights where Registration = ?1 and FlightNumber = ?2 and datetime(LastTime,'30 minutes') > datetime(?3,'unixepoch');";
	sql[TINSFLG] =
	    "insert into Flights (Registration,FlightNumber,StartTime,LastTime) values (?1,?2,datetime(?3,'unixepoch'),datetime(?3,'unixepoch')) ;";
	sql[TUPFLG] =
	    "update Flights set LastTime=datetime(?2,'unixepoch'),NbMessages=NbMessages+1 where FlightID = ?1;";
	sql[TSELMSG] =
	    "select MessageID from Messages where FlightID=?1 and MessNo=?2 ;";
	sql[TINSMSG] =
	    "insert into Messages (FlightID,Time,StID,Channel,Error,SignalLvl,Mode,Ack,Label,BlockNo,MessNo,Txt) values (?1,datetime(?2,'unixepoch'),?3,?4,?5,?6,?7,?8,?9,?10,?11,?12) ;";
	sql[TSELST] =
	    "select StID from Stations where IpAddr = ?1 and IdStation= ?2 ;";
	sql[TINSST] =
	    "insert into Stations (IpAddr,IdStation) values (?1,?2) ;";

	res = sqlite3_exec(acarsdb,
			   "CREATE TABLE IF NOT EXISTS Flights  (FlightID integer primary key,  Registration char(7) ,  FlightNumber char(6),  StartTime datetime,  LastTime datetime, NbMessages integer);\
CREATE INDEX IF NOT EXISTS FlightsFlightNumber on Flights(FlightNumber);\
CREATE INDEX IF NOT EXISTS FlightsRegistration on Flights(Registration);\
CREATE TRIGGER IF NOT EXISTS  MessDel before delete on Flights for each row begin delete from Messages where FlightID = old.FlightID ; end;\
CREATE TABLE IF NOT EXISTS Stations (StID integer primary key,  IdStation varchar, IpAddr varchar );\
CREATE TABLE IF NOT EXISTS Messages (MessageID integer primary key, FlightID integer not null , Time datetime, StID integer, Channel integer , Error integer, SignalLvl integer, Mode char , Ack char , Label char(2), BlockNo char , MessNo char(4) , Txt varchar(250));",
			   NULL, NULL, NULL);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to create tables\n");
		return 1;
	}

	for (i = 0; i < NBTRANS; i++) {

		res = sqlite3_prepare_v2(acarsdb, sql[i], -1, &(stm[i]), NULL);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to preprare %s\n", sql[i]);
			return 1;
		}
	}

	return 0;
}

static sqlite3_int64 updatedb_st(char *ipaddr, char *idst, sqlite3_int64 * sidp)
{
	int res;
	sqlite3_int64 sid;

	if (sidp)
		*sidp = 0;

	res = sqlite3_reset(stm[TSELST]);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to reset %d\n", res);
		return 0;
	}

	res =
	    sqlite3_bind_text(stm[TSELST], 1, ipaddr, strlen(ipaddr),
			      SQLITE_TRANSIENT);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to bind %d\n", res);
		return 0;
	}

	res =
	    sqlite3_bind_text(stm[TSELST], 2, idst, strlen(idst),
			      SQLITE_TRANSIENT);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to bind %d\n", res);
		return 0;
	}

	res = sqlite3_step(stm[TSELST]);
	if (res == SQLITE_DONE) {

		res = sqlite3_reset(stm[TINSST]);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to reset %d\n", res);
			return 0;
		}

		res =
		    sqlite3_bind_text(stm[TINSST], 1, ipaddr, strlen(ipaddr),
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}

		res =
		    sqlite3_bind_text(stm[TINSST], 2, idst, strlen(idst),
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}

		res = sqlite3_step(stm[TINSST]);
		if (res != SQLITE_DONE) {
			fprintf(stderr, "step %d \n", res);
			return 0;
		}
		sid = sqlite3_last_insert_rowid(acarsdb);
	} else if (res == SQLITE_ROW) {

		sid = sqlite3_column_int64(stm[TSELST], 0);

	} else {
		fprintf(stderr, "step %s %d \n", ipaddr, res);
		return 0;
	}

	if (sidp)
		*sidp = sid;
	return 1;
}

static sqlite3_int64 updatedb_fl(char *reg, char *fnum, time_t tm,
				 sqlite3_int64 * fidp)
{
	int res;
	sqlite3_int64 fid;

	if (fidp)
		*fidp = 0;

	res = sqlite3_reset(stm[TSELFLG]);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to reset %d\n", res);
		return 0;
	}

	res =
	    sqlite3_bind_text(stm[TSELFLG], 1, reg, strlen(reg),
			      SQLITE_TRANSIENT);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to bind %d\n", res);
		return 0;
	}

	res =
	    sqlite3_bind_text(stm[TSELFLG], 2, fnum, strlen(fnum),
			      SQLITE_TRANSIENT);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to bind %d\n", res);
		return 0;
	}
	res = sqlite3_bind_int(stm[TSELFLG], 3, (int)tm);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Failed to bind %d\n", res);
		return 0;
	}

	res = sqlite3_step(stm[TSELFLG]);
	if (res == SQLITE_DONE) {

		res = sqlite3_reset(stm[TINSFLG]);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to reset %d\n", res);
			return 0;
		}

		res =
		    sqlite3_bind_text(stm[TINSFLG], 1, reg, strlen(reg),
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSFLG], 2, fnum, strlen(fnum),
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TINSFLG], 3, (int)tm);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}

		res = sqlite3_step(stm[TINSFLG]);
		if (res != SQLITE_DONE) {
			fprintf(stderr, "step %d \n", res);
			return 0;
		}
		fid = sqlite3_last_insert_rowid(acarsdb);
	} else if (res == SQLITE_ROW) {

		fid = sqlite3_column_int64(stm[TSELFLG], 0);

		res = sqlite3_reset(stm[TUPFLG]);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to reset %d\n", res);
			return 0;
		}

		res = sqlite3_bind_int(stm[TUPFLG], 1, fid);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TUPFLG], 2, (int)tm);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_step(stm[TUPFLG]);
		if (res != SQLITE_DONE) {
			fprintf(stderr, "step %d \n", res);
			return 0;
		}

	} else {
		fprintf(stderr, "step %s %d \n", reg, res);
		return 0;
	}

	if (fidp)
		*fidp = fid;
	return 1;
}

static int updatedb_ms(acarsmsg_t * msg, sqlite3_int64 fid, sqlite3_int64 sid,
		       int lm)
{
	int res = SQLITE_OK;

	if ((lm & 4) == 0 && fid != 0) {
		res = sqlite3_reset(stm[TSELMSG]);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to reset %d\n", res);
			return 0;
		}

		res = sqlite3_bind_int(stm[TSELMSG], 1, (int)fid);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}

		res =
		    sqlite3_bind_text(stm[TSELMSG], 2, msg->no, 4,
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}

		res = sqlite3_step(stm[TSELMSG]);
	}

	if ((lm & 4) || fid == 0 || res == SQLITE_DONE) {
		res = sqlite3_reset(stm[TINSMSG]);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to reset %d\n", res);
			return 0;
		}

		res = sqlite3_bind_int(stm[TINSMSG], 1, (int)fid);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TINSMSG], 2, msg->tm);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TINSMSG], 3, sid);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TINSMSG], 4, msg->chn);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TINSMSG], 5, msg->err);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res = sqlite3_bind_int(stm[TINSMSG], 6, msg->lvl);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSMSG], 7, &(msg->mode), 1,
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSMSG], 8, &(msg->ack), 1,
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSMSG], 9, msg->label, 2,
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSMSG], 10, &(msg->bid), 1,
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSMSG], 11, msg->no, 4,
				      SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}
		res =
		    sqlite3_bind_text(stm[TINSMSG], 12, msg->txt,
				      strlen(msg->txt), SQLITE_TRANSIENT);
		if (res != SQLITE_OK) {
			fprintf(stderr, "Failed to bind %d\n", res);
			return 0;
		}

		res = sqlite3_step(stm[TINSMSG]);
		if (res != SQLITE_DONE) {
			fprintf(stderr, "step %d \n", res);
			return 0;
		}
		sqlite3_last_insert_rowid(acarsdb);
	} else {
		return 0;
	}

	return 1;
}

int updatedb(acarsmsg_t * msg, int lm, char *ipaddr)
{
	int res;
	sqlite3_int64 fid, sid;

	res = sqlite3_step(stm[TBT]);
	if (res != SQLITE_DONE) {
		fprintf(stderr, "step %d \n", res);
		return 1;
	}
	if (updatedb_st(ipaddr, msg->idst, &sid) == 0) {
		sqlite3_step(stm[TRL]);
		return 1;
	}

	fid = 0;
	if (lm & 1)
		if (updatedb_fl(msg->reg, msg->fid, msg->tm, &fid) == 0) {
			sqlite3_step(stm[TRL]);
			return 1;
		}

	if (lm & 2)
		if (updatedb_ms(msg, fid, sid, lm) == 0) {
			sqlite3_step(stm[TRL]);
			return 1;
		}

	sqlite3_step(stm[TET]);
	return 0;
}
