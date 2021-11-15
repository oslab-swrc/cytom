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

#include <assert.h>
#include <chrono>

using namespace llvm;

namespace{

struct ChangeFuncName : public ModulePass{
  static char ID;

  ChangeFuncName():ModulePass(ID){}

  virtual bool runOnModule(Module &M){
    auto &functionList = M.getFunctionList();
    for (auto &F : functionList) {
      if (F.getName() == "main"){
        errs() << "Hello: ";
        errs().write_escaped(F.getName()) << '\n';
	
	uint64_t epoch_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::string new_name("new_main_" + std::to_string(epoch_time));
        Twine new_twine(new_name);
        F.setName(new_twine);

        errs() << "Hello: ";
        errs().write_escaped(F.getName()) << '\n';
        break;
      }   
    }
    return true;
  }
};
}

char ChangeFuncName::ID=0;
static RegisterPass<ChangeFuncName> X("changeFuncName", "Change Fuction Name Pass");

namespace{

struct MergeMain : public ModulePass{
  static char ID;

  MergeMain():ModulePass(ID){}

  virtual bool runOnModule(Module &M){
    auto &functionList = M.getFunctionList();
    std::vector<Function*> exe_funcs;
    std::vector<Function*> new_main_funcs;

    for (auto &F : functionList) {
      if (F.getName().contains("new_main_")){
        new_main_funcs.push_back(&F);
        for (BasicBlock &BB : F) {
          for (Instruction &inst : BB) {
            if(isa<CallInst>(&inst)) { 
              // get inst of function call (of execute<Algorithm>)
      	      if(cast<CallInst>(inst).getCalledFunction()->getName().contains("execute")) {
      		      errs() << "callinst => " << inst << "\n";
      		      Function * exe_func = cast<CallInst>(inst).getCalledFunction();
                    	      exe_funcs.push_back(exe_func);
      	      }
            }
          }
        }
      }   
    }

    for (auto &F : functionList) {
      if (F.getName() == "main"){
        std::vector<Value *> args;
      	for(auto arg = F.arg_begin(); arg != F.arg_end(); ++arg) {
      		args.push_back(arg);
      		errs() << "arg => " << *arg << "\n";
      	}

        if(args.size() != 2) {
          errs() << "[ERROR]wrong args size: " << args.size() << "\n";
          return false;
        }

        for (inst_iterator I = inst_begin(F), E= inst_end(F); I != E; ++I) {
          Instruction *inst = &*I;
          // if(isa<CallInst>(inst) && cast<CallInst>(*inst).getCalledFunction()->getName().contains("execute")) {
          if(isa<ReturnInst>(inst)) {
            errs() << "returnInst => " << *inst << "\n";
            for(auto * exe_func : exe_funcs) {
              IRBuilder<> builder(inst);
              builder.CreateCall(exe_func, args);
            }
          }
        }
      }   
    }

    // remove all extra main function
    for(auto * F : new_main_funcs) {
      errs() << "delete function: " << F->getName() << "\n";
      F->eraseFromParent();
    }

    return true;
  }
};
}

char MergeMain::ID=0;
static RegisterPass<MergeMain> Y("mergeMain", "Merge Multiple Main Functions Pass");


namespace{

struct HoldDataInMem : public ModulePass{
  static char ID;

  HoldDataInMem():ModulePass(ID){}

  virtual bool runOnModule(Module &M){
    GlobalVariable *fileEdges;
    // assign value to file_edges when execute initEdgeGenerators the first time
    auto &functionList = M.getFunctionList();
    bool set_global = false;
    for (auto &F : functionList) {
      if (!set_global &&F.getName().contains("initEdgeGenerators")){
        errs() << "In Function: " << F.getName() << "\n";
        for (inst_iterator I = inst_begin(F), E= inst_end(F); I != E; ++I) {
          Instruction *inst = &*I;
          if(isa<CallInst>(inst) && cast<CallInst>(*inst).getCalledFunction()->getName() == "mmap") {
            errs() << "CallInst(mmap) => " << *inst << "\n";

            Instruction *bitcast_inst = inst->getNextNode();
            assert(isa<BitCastInst>(bitcast_inst));
            errs() << "BitcastlInst => " << *bitcast_inst << "\n";

            IRBuilder<> builder(bitcast_inst->getNextNode());

            // create global variable file_edges
            fileEdges = new GlobalVariable(
              M, 
              bitcast_inst->getType(),
              false,
              GlobalValue::ExternalLinkage,
              Constant::getNullValue(bitcast_inst->getType()),
              "file_edges");
            fileEdges->setAlignment(4);

            // store file_edges
            builder.CreateStore(bitcast_inst, fileEdges, false);

            errs() << "SET GLOBAL\n";

            set_global = true;
            break;
          }
        }

        if(set_global)
          continue;
      }

      if(set_global && F.getName().contains("initEdgeGenerators")) {
        errs() << "In Function: " << F.getName() << "\n";

        // initEdgeGenerators in other algorithms (not the first)
        for (inst_iterator I = inst_begin(F), E= inst_end(F); I != E; ++I) {
          Instruction *inst = &*I;
          if(isa<CallInst>(inst) && cast<CallInst>(*inst).getCalledFunction()->getName() == "mmap") {
            errs() << "CallInst(mmap) => " << *inst << "\n";

            Instruction *bitcast_inst = inst->getNextNode();
            assert(isa<BitCastInst>(bitcast_inst));
            errs() << "BitcastlInst => " << *bitcast_inst << "\n";

            // bitcast file_edges from array pointer to (first element's) pointer
            IRBuilder<> builder(bitcast_inst);
            auto * fileEdgesLocal = builder.CreateBitCast(fileEdges, bitcast_inst->getType(), "file_edges_local");

            // replace all use of bitcast_inst with file_edges
            bitcast_inst->replaceAllUsesWith(fileEdgesLocal);

            // clean up locals to save data i/o
            bitcast_inst->eraseFromParent();

            errs() << "Replace GLOBAL\n";

            break;
          }
        }
      }   
    }

    return true;
  }
};
}

char HoldDataInMem::ID=0;
static RegisterPass<HoldDataInMem> Z("holdDataInMem", "Hold Input Data In Memory Pass");
