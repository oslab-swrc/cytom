// SPDX-License-Identifier: MIT

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constant.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/BasicBlock.h"

#include <assert.h>
#include <chrono>
#include <vector>
#include <set>

using namespace llvm;

/* No need anymore; ask clients to provide "main_algo-name" function */
// static cl::opt<std::string> JobIndex("jobIndex", cl::desc("Specify job index for changeMainFuncName pass"), cl::value_desc("job index"));

// namespace{

// struct ChangeMainFuncName : public ModulePass{
//   static char ID;

//   ChangeMainFuncName():ModulePass(ID){}

//   virtual bool runOnModule(Module &M){
//     auto &functionList = M.getFunctionList();
//     for (auto &F : functionList) {
//       if (F.getName() == "main"){
//         errs() << "Hello: ";
//         errs().write_escaped(F.getName()) << '\n';
	
//         // uint64_t epoch_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
//         // std::string new_name("new_main_" + std::to_string(epoch_time));
//         std::string new_name("new_main_" + JobIndex);
//         Twine new_twine(new_name);
//         F.setName(new_twine);

//         errs() << "Hello: ";
//         errs().write_escaped(F.getName()) << '\n';
//         break;
//       }   
//     }
//     return true;
//   }
// };
// }

// char ChangeMainFuncName::ID=0;
// static RegisterPass<ChangeMainFuncName> X("changeMainFuncName", "Change Main Fuction Name Pass");

cl::list<std::string> Jobs("jobs", cl::desc("Specify job names"), cl::OneOrMore);

namespace{

struct ComposeJobs : public ModulePass{
  static char ID;

  ComposeJobs():ModulePass(ID){}

  bool runOnModule(Module &M) override{
    auto &functionList = M.getFunctionList();
    std::vector<Function*> job_funcs(Jobs.size());
    // errs() << "Number of jobs: " << Jobs.size() << "\n";
    std::set<Function*> algo_main_funcs;

    // collect job_funcs & algo_main_funcs
    for (auto &F : functionList) {
      for(int i = 0; i < Jobs.size(); i++) {
        if (F.getName().contains("main_" + Jobs[i])){
          algo_main_funcs.insert(&F);
          for (BasicBlock &BB : F) {
            for (Instruction &inst : BB) {
              if(isa<CallInst>(&inst)) { 
                // get inst of function call (of execute<Algorithm>)
                if(cast<CallInst>(inst).getCalledFunction()->getName().contains("execute")) {
                  // errs() << "callinst => " << inst << "\n";
                  Function * exe_func = cast<CallInst>(inst).getCalledFunction();
                  job_funcs[i] = exe_func;
                }
              }
            }
          }
        }
      }
    }

    /* 
      1. create a BB to invoke add_job function for each job
      2. replace dummy_func with the job_func
      3. set successors (normal dest) of BB
    */

    Function * main_func = M.getFunction("main");
    InvokeInst * last_job; // record last InvokeInst (to set its normal dest)

    Function * add_job_func;
    Value * job_arg_1;
    BasicBlock *  normal_dest;
    BasicBlock *  unwind_dest;
    for (inst_iterator I = inst_begin(*main_func), E= inst_end(*main_func); I != E; ++I) {
      Instruction *inst = &*I;
      if(isa<InvokeInst>(inst) && cast<InvokeInst>(inst)->getCalledFunction()->getName().contains("add_job")) {
        auto * add_job_inst = cast<InvokeInst>(inst);
        add_job_func = add_job_inst->getCalledFunction();
        job_arg_1 = add_job_inst->getArgOperand(0);
      
        normal_dest = add_job_inst->getNormalDest();
        unwind_dest = add_job_inst->getUnwindDest();

        // set first job
        last_job = add_job_inst;
        last_job->setOperand(1, job_funcs[0]); 
        break;
      }
    }

    for(int cur_job = 1; cur_job < Jobs.size(); cur_job++) {
      auto * job_block = BasicBlock::Create(M.getContext(), "job_" + std::to_string(cur_job), main_func);
      
      IRBuilder<> builder(job_block);
      builder.SetInsertPoint(job_block);

      std::vector<Value *> job_args;
      job_args.push_back(job_arg_1);
      job_args.push_back(job_funcs[cur_job]);
      auto * new_inst = builder.CreateInvoke(add_job_func, normal_dest, unwind_dest, job_args);

      // set last_job's normal dest
      last_job->setNormalDest(job_block);
      last_job = new_inst;
    }

    // remove all extra main function
    for(auto * F : algo_main_funcs) {
      // errs() << "delete function: " << F->getName() << "\n";
      F->eraseFromParent();
    }

    return false;
  }
};
}

char ComposeJobs::ID=0;
static RegisterPass<ComposeJobs> Y("composeJobs", "Compose Jobs Pass", false, false);

