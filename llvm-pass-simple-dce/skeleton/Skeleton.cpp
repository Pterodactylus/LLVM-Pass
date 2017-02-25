<<<<<<< HEAD
#define DEBUG_TYPE "simpledce"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/ADT/SmallVector.h"

#include <map>

using namespace llvm;

typedef std::set<Instruction*> InstrSet;

namespace {
   struct SimpleDCE : public FunctionPass {
     static char ID;
     SimpleDCE() : FunctionPass(ID) {}

     virtual void instructionsCount(Function &F) {
         std::map<std::string, int> opsCounter;

          for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
              if (opsCounter.find(I -> getOpcodeName()) == opsCounter.end()) {
                  opsCounter[I -> getOpcodeName()] = 1;
              } else {
                  opsCounter[I -> getOpcodeName()] += 1;
              }
          }

          std::map<std::string, int>::iterator i = opsCounter.begin();
          std::map<std::string, int>::iterator e = opsCounter.end();

          while (i != e) {
              errs() << i -> first << ": " << i -> second << "\n";
              i++;
          }
          opsCounter.clear();
      }
     
     
     virtual bool runOnFunction(Function &F) {
         errs() << "BEFORE DCE\n";

         instructionsCount(F);

         //Count before dead code elimination here
         errs() << "DCE START\n";

         SmallVector<Instruction*, 64> WorkList;

         // Just add all instructions to the work list.

         for (inst_iterator i = inst_begin(F); i != inst_end(F); ++i) {
             WorkList.push_back(&*i);
         }

         // Then run the worklist algorithm.

         while (!WorkList.empty()) {
             Instruction* instr = WorkList.pop_back_val();

             if (isInstructionTriviallyDead(instr)) {

                 // Push back all operands that are instructions.
                 for (Use &I : instr->operands()) {
                     if (Instruction* i = dyn_cast<Instruction>(I)) {
                         WorkList.push_back(i);
                     }
                 }


                 // Don't forget to delete the instruction.
                 instr->eraseFromParent();

                 // Delete dead instruction if it is repeated in the work list.
                 for (SmallVector<Instruction*, 64>::iterator WI = WorkList.begin(); WI != WorkList.end(); ) {
                    if (*WI == instr) {
                        WI = WorkList.erase(WI);
                    } else {
                        ++WI;
                    }
                 }
             }
         }
  
         //Eliminate dead code and produce counts here
         //(You can use is instruction trivially dead)

         instructionsCount(F);
         errs() << "DCE END\n";
         return false;
     }
   };
}
char SimpleDCE::ID = 0;
static RegisterPass<SimpleDCE> X("simpledce", "Simple dead code elimination");

=======
#define DEBUG_TYPE "simpledce"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/ADT/SmallVector.h"

#include <map>

using namespace llvm;

typedef std::set<Instruction*> InstrSet;

namespace {
   struct SimpleDCE : public FunctionPass {
     static char ID;
     SimpleDCE() : FunctionPass(ID) {}

     virtual void instructionsCount(Function &F) {
         std::map<std::string, int> opsCounter;

          for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
              if (opsCounter.find(I -> getOpcodeName()) == opsCounter.end()) {
                  opsCounter[I -> getOpcodeName()] = 1;
              } else {
                  opsCounter[I -> getOpcodeName()] += 1;
              }
          }

          std::map<std::string, int>::iterator i = opsCounter.begin();
          std::map<std::string, int>::iterator e = opsCounter.end();

          while (i != e) {
              errs() << i -> first << ": " << i -> second << "\n";
              i++;
          }
          opsCounter.clear();
      }
     
     
     virtual bool runOnFunction(Function &F) {
         errs() << "BEFORE DCE\n";

         instructionsCount(F);

         //Count before dead code elimination here
         errs() << "DCE START\n";

         SmallVector<Instruction*, 64> WorkList;

         // Just add all instructions to the work list.

         for (inst_iterator i = inst_begin(F); i != inst_end(F); ++i) {
             WorkList.push_back(&*i);
         }

         // Then run the worklist algorithm.

         while (!WorkList.empty()) {
             Instruction* instr = WorkList.pop_back_val();

             if (isInstructionTriviallyDead(instr)) {

                 // Push back all operands that are instructions.
                 for (Use &I : instr->operands()) {
                     if (Instruction* i = dyn_cast<Instruction>(I)) {
                         WorkList.push_back(i);
                     }
                 }


                 // Don't forget to delete the instruction.
                 instr->eraseFromParent();

                 // Delete dead instruction if it is repeated in the work list.
                 for (SmallVector<Instruction*, 64>::iterator WI = WorkList.begin(); WI != WorkList.end(); ) {
                    if (*WI == instr) {
                        WI = WorkList.erase(WI);
                    } else {
                        ++WI;
                    }
                 }
             }
         }
  
         //Eliminate dead code and produce counts here
         //(You can use is instruction trivially dead)

         instructionsCount(F);
         errs() << "DCE END\n";
         return false;
     }
   };
}
char SimpleDCE::ID = 0;
static RegisterPass<SimpleDCE> X("simpledce", "Simple dead code elimination");

>>>>>>> 495f456b4d4a00cf2701b1796cce8b8d0a0958fc
