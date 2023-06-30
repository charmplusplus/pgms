#include <stdio.h>
#include <time.h>
#include <converse.h>
#include "bigmsg.cpm.h"

CpvDeclare(int, bigmsg_index);
CpvDeclare(int, shortmsg_index);
CpvDeclare(int, msg_size);
CpvDeclare(int, cycle);
CpvDeclare(int, recv_count);
CpvDeclare(int, ack_count);
CpvDeclare(double, total_time);
CpvDeclare(double, process_time);
CpvDeclare(double, send_time);

#define MSG_SIZE 8
#define MSG_COUNT 100
#define nCycles 8

typedef struct myMsg
{
  char header[CmiMsgHeaderSizeBytes];
  int payload[1];
} *message;

CpmInvokable bigmsg_stop()
{
  CsdExitScheduler();
}

void send_msg() {
  int i, k;
  double start_time, crt_time;
  struct myMsg *msg;
//  CmiPrintf("\nSending msg fron pe%d to pe%d\n",CmiMyPe(), CmiNumPes()/2+CmiMyPe());
  CpvAccess(process_time) = 0.0;
  CpvAccess(send_time) = 0.0;
  CpvAccess(total_time) = CmiWallTimer();
  for(k=0;k<MSG_COUNT;k++) {
    crt_time = CmiWallTimer();
    msg = (message)CmiAlloc(CpvAccess(msg_size));
    CmiSetHandler(msg, CpvAccess(bigmsg_index));
    CpvAccess(process_time) = CmiWallTimer() - crt_time + CpvAccess(process_time);
    start_time = CmiWallTimer();
    //Send from my pe-i on node-0 to q+i on node-1
    CmiSyncSendAndFree(CmiNumPes()/2+CmiMyPe(), CpvAccess(msg_size), msg);
    CpvAccess(send_time) = CmiWallTimer() - start_time + CpvAccess(send_time);
  }
}


void shortmsg_handler(void *vmsg) {
  message smsg = (message)vmsg;
  CmiFree(smsg);
  CpvAccess(msg_size) = (CpvAccess(msg_size)-CmiMsgHeaderSizeBytes)*2+CmiMsgHeaderSizeBytes;
  send_msg();
}

void bigmsg_handler(void *vmsg)
{
  int i, next, pe;
  message msg = (message)vmsg;
  if (CmiMyPe()>=CmiNumPes()/2) {
    CpvAccess(recv_count) = 1 + CpvAccess(recv_count);
    if(CpvAccess(recv_count) == MSG_COUNT) {
      CpvAccess(recv_count) = 0;
      CmiFree(msg);
      msg = (message)CmiAlloc(CpvAccess(msg_size));
      CmiSetHandler(msg, CpvAccess(bigmsg_index));
      CmiSyncSendAndFree(0, CpvAccess(msg_size), msg);
    } else
      CmiFree(msg);
  } else { //Pe-0 receives all acks
    CpvAccess(ack_count) = 1 + CpvAccess(ack_count);
    if(CpvAccess(ack_count) == CmiNumPes()/2) {
      CpvAccess(ack_count) = 0;
      CpvAccess(total_time) = CmiWallTimer() - CpvAccess(total_time);
      CmiPrintf("\nReceived [Cycle=%d, msg size=%d] ack on PE-#%d send time=%lf, process time=%lf, total time=%lf",
              CpvAccess(cycle), CpvAccess(msg_size), CmiMyPe(), CpvAccess(send_time), CpvAccess(process_time), CpvAccess(total_time));
      CmiFree(msg);
      CpvAccess(cycle) = CpvAccess(cycle)+1;
      if(CpvAccess(cycle) == nCycles)
        Cpm_bigmsg_stop(CpmSend(CpmALL));
      else {
 //       CmiPrintf("\nSending short msgs from PE-%d", CmiMyPe());
        for(pe=0;pe<CmiNumPes()/2;pe++) {
          int smsg_size = 4+CmiMsgHeaderSizeBytes;
          message smsg = (message)CmiAlloc(smsg_size);
          CmiSetHandler(smsg, CpvAccess(shortmsg_index));
          CmiSyncSendAndFree(pe, smsg_size, smsg);
        }
      }
    }
  }
}

void bigmsg_init()
{
  int totalpes = CmiNumPes(); //p=num_pes
  int pes_per_node = totalpes/2; //q=p/2
  if (CmiNumPes()%2 !=0) {
    CmiPrintf("note: this test requires at multiple of 2 pes, skipping test.\n");
    CmiPrintf("exiting.\n");
    CsdExitScheduler();
    Cpm_bigmsg_stop(CpmSend(CpmALL));
  } else {
    if(CmiMyPe() < pes_per_node)
      send_msg();
  }
}

void bigmsg_moduleinit(int argc, char **argv)
{
  CpvInitialize(int, bigmsg_index);
  CpvInitialize(int, shortmsg_index);
  CpvInitialize(int, msg_size);
  CpvInitialize(int, cycle);
  CpvInitialize(int, recv_count);
  CpvInitialize(int, ack_count);
  CpvInitialize(double, total_time);
  CpvInitialize(double, send_time);
  CpvInitialize(double, process_time);

  CpvAccess(bigmsg_index) = CmiRegisterHandler(bigmsg_handler);
  CpvAccess(shortmsg_index) = CmiRegisterHandler(shortmsg_handler);
  CpvAccess(msg_size) = 16+CmiMsgHeaderSizeBytes;
  CpvAccess(cycle) = 1;
  void CpmModuleInit(void);
  void CfutureModuleInit(void);
  void CpthreadModuleInit(void);

  CpmModuleInit();
  CfutureModuleInit();
  CpthreadModuleInit();
  CpmInitializeThisModule();
  // Set runtime cpuaffinity
  CmiInitCPUAffinity(argv);
  // Initialize CPU topology
  CmiInitCPUTopology(argv);
  // Wait for all PEs of the node to complete topology init
  CmiNodeAllBarrier();

  // Update the argc after runtime parameters are extracted out
  argc = CmiGetArgc(argv);
  if(CmiMyPe() < CmiNumPes()/2)
    bigmsg_init();
}

int main(int argc, char **argv)
{
  ConverseInit(argc,argv,bigmsg_moduleinit,0,0);
}

