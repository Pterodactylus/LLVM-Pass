<<<<<<< HEAD
#define DEBUG_TYPE "mydce"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <map>

using namespace llvm;

typedef std::set<Instruction*>* InstrSet;
typedef std::set<Instruction*>::iterator InstrIter;

namespace {
    struct MyDCE: public FunctionPass {
        static char ID;
        MyDCE(): FunctionPass(ID) {}

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

        virtual void initializeLiveSets(Function &F,
            std::set<BasicBlock*> &bbWorkSet,
            std::map<BasicBlock*, InstrSet> &bbLiveBefore,
            std::map<BasicBlock*, InstrSet> &bbLiveAfter) {

            Function::iterator fit;
            BasicBlock::iterator bbit;

            for (fit = F.begin(); fit != F.end(); ++fit) {
                BasicBlock* blk = &*fit;
                bbWorkSet.insert(blk);

                InstrSet liveBefore = new std::set<Instruction*>();
                bbLiveBefore.insert(std::pair<BasicBlock*, InstrSet>(blk, liveBefore));

                InstrSet liveAfter = new std::set<Instruction*>();
                bbLiveAfter.insert(std::pair<BasicBlock*, InstrSet>(blk, liveAfter));
            }
        }

        virtual bool runOnFunction(Function &F) {
            errs() << "BEFORE DCE\n";
            //Count before dead code elimination here
            instructionsCount(F);

            //Implement your live variable analysis here
            errs() << "DCE START\n";

            /*
             * The solution is based on the following link: http://pages.cs.wisc.edu/~fischer/cs701.f14/proj2.html
             * 
             * The procedure outlined there computes live before/after sets for each block, then uses the information
             * to iterate backwards over instructions in each block, and compute the same sets for individual instructions.
             * 
             * However, the project description in above link, while providing good clues on how to implement the liveness
             * analysis, does not include the following requirements:
             *
             *   1. Only removes dead instructions once and then does not investigate whether new dead instructions appeared after
             *      removal.
             *   2. Does not account for interdependency of instructions between basic blocks. The removal only works locally at basic
             *      block level.
             *
             *   To amend 1., if instruction is dead, i.e. it's pseudo-register is not in the live after set and it has no side effects,
             *   then instead of using data flow equation to compute live before set, we copy live before set of LAST LIVE INSTRUCTION.
             *   The reason for it is that since dead instruction is irrelevant, then we only care about last live successor's before set.
             *
             *   To amend 2: employ similar idea as in simple DCE. Do whole iterative computation on all basic blocks, remove dead instructions.
             *   Keep track of whether any instructions were added to useless instructions set for removal. If that is the case, then run the analysis
             *   again. When no new instructions were added to useless set in iteration X, it means individual dead instructions were removed from basic
             *   blocks and there are no more dependencies between them.
             **/

            //Eliminate dead code and produce counts here

            while (true) {
                bool changed = false; // Will be swapped to true, if any dead instructions in any block were encountered.

                std::set<BasicBlock*> bbWorkSet;

                std::map<BasicBlock*, InstrSet> bbLiveBefore;
                std::map<BasicBlock*, InstrSet> bbLiveAfter;

                std::map<Instruction*, InstrSet> instLiveBefore;
                std::map<Instruction*, InstrSet> instLiveAfter;

                initializeLiveSets(F, bbWorkSet, bbLiveBefore, bbLiveAfter);

                // Run WorkList algorithm on BasicBlocks.
                while (!bbWorkSet.empty()) {
                    BasicBlock* blk = *(bbWorkSet.begin()); // Gets first element in the set.
                    bbWorkSet.erase(blk); // Remove used block from the set.

                    InstrSet genSet = new std::set<Instruction*>();
                    InstrSet killSet = new std::set<Instruction*>();

                    computeBasicBlockGenKillSets(blk, genSet, killSet);

                    // InstrSet genSet = computeBBGenSet(blk);
                    // InstrSet killSet = computeBBKillSet(blk);

                    InstrSet liveAfter = computeBBLiveAfterSet(blk, bbLiveBefore);

                    // Assign new live after set to basic block.
                    delete bbLiveAfter[blk];
                    bbLiveAfter[blk] = liveAfter;

                    // Compute new live before set using new live after, gen and kill sets.
                    InstrSet liveBefore = bbLiveBefore[blk];
                    InstrSet newLiveBefore = computeNewLiveBefore(liveAfter, genSet, killSet);

                    if (!(*liveBefore == *newLiveBefore)) {
                        delete liveBefore;
                        bbLiveBefore[blk] = newLiveBefore;
                        for (pred_iterator pit = pred_begin(blk); pit != pred_end(blk); pit++) {
                            bbWorkSet.insert(*pit);
                        }
                    } else {
                        delete newLiveBefore;
                    }

                    delete genSet;
                    delete killSet;
                }

                // Compute live before/after sets for each instruction in the block. Account for dead instructions.

                computeInstructionLiveSets(F, bbLiveBefore, bbLiveAfter, instLiveBefore, instLiveAfter);

                // Iterate over all instructions and collect dead instructions.
                InstrSet deadInstrs = new std::set<Instruction*>();
                for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                    Instruction* instr = &*I;
                    if (isDeadInstruction(instr) && !instLiveAfter[instr]->count(instr)) {
                        changed = true;
                        // Add dead instruction for removal.
                        deadInstrs->insert(instr);
                    }
                }

                // Remove dead instruction
                for (InstrIter it = deadInstrs->begin(); it != deadInstrs->end(); ++it) {
                    (*it)->eraseFromParent();
                }

                if (!changed) break;

            }

            instructionsCount(F);

            errs() << "DCE END\n";

            return true; // Probably changed smth.
        }

        virtual void computeBasicBlockGenKillSets(BasicBlock* blk, InstrSet &genSet, InstrSet &killSet) {
            BasicBlock::iterator bit;
            for (bit = blk->begin(); bit != blk->end(); ++bit) {
                Instruction* i = &*bit;
                int n = i->getNumOperands();
                for (int j = 0; j < n; j++) {
                    Value* usedValue = i->getOperand(j);
                    if (isa<Instruction>(usedValue)) {
                        Instruction* op = cast<Instruction>(usedValue);
                        if (!killSet->count(op)) {
                            genSet->insert(op);
                        }
                    }
                }
                killSet->insert(i);
            }
        }

        virtual void computeInstructionLiveSets(Function &F,
                                                std::map<BasicBlock*, InstrSet> &bbLiveBefore,
                                                std::map<BasicBlock*, InstrSet> &bbLiveAfter,
                                                std::map<Instruction*, InstrSet> &instLiveBefore,
                                                std::map<Instruction*, InstrSet> &instLiveAfter) {
            Function::iterator fit;
            BasicBlock::iterator bit;
            for (fit = F.begin(); fit != F.end(); ++fit) {
                // Iterate all instructions in the block backwards.
                BasicBlock* blk = &*fit;
                bit = blk->end();

                Instruction* current, *next, *lastSuccessful;
                bool terminator = true; // Last instruction in the block is terminator instruction.
                do {
                    --bit;
                    next = current;
                    current = &*bit;

                    instLiveAfter[current] = new std::set<Instruction*>(); // Prepare instruction live after set.

                    if (terminator) {
                        // Terminator instruction live after set is the same block live after set.
                        *(instLiveAfter[current]) = *(bbLiveAfter[blk]);
                        lastSuccessful = current;

                        terminator = false;
                    } else {
                        // Otherwise, copy live before of successor instruction.
                        *(instLiveAfter[current]) = *(instLiveBefore[next]);
                    }

                    // Instruction is dead if it's live after set does not contain it and it is one of the
                    // possible instruction without side effects.

                    if (!instLiveAfter[current]->count(current) && isDeadInstruction(current)) {
                        // In this case, copy last successful instruction liveBefore set to
                        // current instruction's liveBefore set.
                        instLiveBefore[current] = instLiveBefore[lastSuccessful];
                    } else {
                        // Otherwise, instruction is live, so just use data flow equation
                        // to compute instruction's live before set.

                        // Also, note that this is not most recent successful live instruction.
                        lastSuccessful = current;

                        instLiveBefore[current] = computeInstructionLiveBefore(current, instLiveAfter);
                    }
                } while (bit != blk->begin());
            }
        }

        virtual InstrSet computeInstructionLiveBefore(Instruction* I,
                                                      std::map<Instruction*, InstrSet> &instLiveAfter) {
            // Start with empty set. Add all instructions of live after set. Then delete kill set
            // instruction. Finally add all operands of instruction that are instructions.

            InstrSet instrLiveBefore = new std::set<Instruction*>();

            *instrLiveBefore = *(instLiveAfter[I]);

            instrLiveBefore->erase(I); // Delete kill set.

            // Add all instruction's operands that are instructions. Gen set.
            User::op_iterator opit;
            for (opit = I->op_begin(); opit != I->op_end(); opit++)
            {
                Use *use = &*opit;
                Value *usedValue = use->get();
                if (isa<Instruction>(usedValue)) {
                    Instruction *usedInstr = cast<Instruction>(usedValue);
                    if (usedInstr != NULL) {
                        instrLiveBefore->insert(usedInstr);
                    }
                }
            }

            return instrLiveBefore;
        }

        virtual bool isDeadInstruction(Instruction* I) {
            return I->isBinaryOp() ||
                   I->isCast() ||
                   I->isShift() ||
                   I->getOpcode() == Instruction::Alloca ||
                   I->getOpcode() == Instruction::Load ||
                   I->getOpcode() == Instruction::GetElementPtr ||
                   I->getOpcode() == Instruction::Select ||
                   I->getOpcode() == Instruction::ExtractElement ||
                   I->getOpcode() == Instruction::ExtractValue ||
                   I->getOpcode() == Instruction::ICmp ||
                   I->getOpcode() == Instruction::FCmp;
        }

        virtual InstrSet computeNewLiveBefore(InstrSet liveAfter,
                                              InstrSet genSet,
                                              InstrSet killSet) {
            InstrSet newLiveBefore = new std::set<Instruction*>();

            // LiveBefore = genSet UNION (liveAfter DIFFERENCE killSet)

            // Firstly, insert liveAfter into newLiveBefore
            for (InstrIter it = liveAfter->begin(); it != liveAfter->end(); ++it) {
                newLiveBefore->insert(*it);
            }

            // Then remove all elements from liveAfter that are in killSet aka set difference.
            for (InstrIter it = killSet->begin(); it != killSet->end(); ++it) {
                newLiveBefore->erase(*it);
            }

            // Add all items from genSet into newLiveBefore set.
            for (InstrIter it = genSet->begin(); it != genSet->end(); ++it) {
                newLiveBefore->insert(*it);
            }

            return newLiveBefore;

        }

        virtual InstrSet computeBBKillSet(BasicBlock* blk) {
            // The kill set of basic block is a combination of all kill sets for each instruction.
            // The kill set for instruction is an instruction itself.
            InstrSet killSet = new std::set<Instruction*>();

            BasicBlock::iterator bit;
            for (bit = blk->begin(); bit != blk->end(); ++bit) {
                killSet->insert(&*bit);
            }
            return killSet;
        }

        virtual InstrSet computeBBGenSet(BasicBlock* blk) {
            // The gen set for the basic block are all unique uses of instruction
            // in each instruction.
            InstrSet genSet = new std::set<Instruction*>();

            BasicBlock::iterator bit;

            InstrSet seen = new std::set<Instruction*>();

            for (bit = blk->begin(); bit != blk->end(); ++bit) {
                Instruction* I = &*bit;

                User::op_iterator opit;
                for (opit = I->op_begin(); opit != I->op_end(); opit++) {
                    Use* use = &*opit;
                    Value* usedValue = use->get();
                    if (isa<Instruction>(usedValue)) {
                        // The Value is an Instruction.
                        Instruction* usedInstr = cast<Instruction>(usedValue);

                        if (!seen->count(usedInstr)) {
                            genSet->insert(usedInstr);
                        }
                    }
                }
                seen->insert(I);
            }

            delete seen;

            return genSet;
        }

        virtual InstrSet computeBBLiveAfterSet(BasicBlock* blk,
                                               std::map<BasicBlock*, InstrSet> &bbLiveBefore) {
            InstrSet liveAfter = new std::set<Instruction*>();
            for (succ_iterator s_it = succ_begin(blk); s_it != succ_end(blk); s_it++) {
                InstrSet basicBlockLiveBefore = bbLiveBefore[*s_it];
                std::set<Instruction*>::iterator it;
                for (InstrIter it = basicBlockLiveBefore->begin(); it != basicBlockLiveBefore->end(); it++) {
                    liveAfter->insert(*it);
                }
            }
            return liveAfter;
        }
    };
}
char MyDCE::ID = 0;
=======
#define DEBUG_TYPE "opCounter"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <map>

using namespace llvm;

typedef std::set<Instruction*>* InstrSet;
typedef std::set<Instruction*>::iterator InstrIter;

namespace {
    struct MyDCE: public FunctionPass {
        static char ID;
        MyDCE(): FunctionPass(ID) {}

        virtual void instructionsCount(Function & F) {
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

        virtual void initializeLiveSets(Function &F,
            std::set<BasicBlock*> &bbWorkSet,
            std::map<BasicBlock*, InstrSet> &bbLiveBefore,
            std::map<BasicBlock*, InstrSet> &bbLiveAfter) {

            Function::iterator fit;
            BasicBlock::iterator bbit;

            for (fit = F.begin(); fit != F.end(); ++fit) {
                BasicBlock* blk = &*fit;
                bbWorkSet.insert(blk);

                InstrSet liveBefore = new std::set<Instruction*>();
                bbLiveBefore.insert(std::pair<BasicBlock*, InstrSet>(blk, liveBefore));

                InstrSet liveAfter = new std::set<Instruction*>();
                bbLiveAfter.insert(std::pair<BasicBlock*, InstrSet>(blk, liveAfter));
            }
        }

        virtual bool runOnFunction(Function &F) {
            errs() << "BEFORE DCE\n";
            //Count before dead code elimination here
            instructionsCount(F);

            //Implement your live variable analysis here
            errs() << "DCE START\n";

            /*
             * The solution is based on the following link: http://pages.cs.wisc.edu/~fischer/cs701.f14/proj2.html
             * 
             * The procedure outlined there computes live before/after sets for each block, then uses the information
             * to iterate backwards over instructions in each block, and compute the same sets for individual instructions.
             * 
             * However, the project description in above link, while providing good clues on how to implement the liveness
             * analysis, does not include the following requirements:
             *
             *   1. Only removes dead instructions once and then does not investigate whether new dead instructions appeared after
             *      removal.
             *   2. Does not account for interdependency of instructions between basic blocks. The removal only works locally at basic
             *      block level.
             *
             *   To amend 1., if instruction is dead, i.e. it's pseudo-register is not in the live after set and it has no side effects,
             *   then instead of using data flow equation to compute live before set, we copy live before set of LAST LIVE INSTRUCTION.
             *   The reason for it is that since dead instruction is irrelevant, then we only care about last live successor's before set.
             *
             *   To amend 2: employ similar idea as in simple DCE. Do whole iterative computation on all basic blocks, remove dead instructions.
             *   Keep track of whether any instructions were added to useless instructions set for removal. If that is the case, then run the analysis
             *   again. When no new instructions were added to useless set in iteration X, it means individual dead instructions were removed from basic
             *   blocks and there are no more dependencies between them.
             **/

            //Eliminate dead code and produce counts here

            while (true) {
                bool changed = false; // Will be swapped to true, if any dead instructions in any block were encountered.

                std::set<BasicBlock*> bbWorkSet;

                std::map<BasicBlock*, InstrSet> bbLiveBefore;
                std::map<BasicBlock*, InstrSet> bbLiveAfter;

                std::map<Instruction*, InstrSet> instLiveBefore;
                std::map<Instruction*, InstrSet> instLiveAfter;

                initializeLiveSets(F, bbWorkSet, bbLiveBefore, bbLiveAfter);

                // Run WorkList algorithm on BasicBlocks.
                while (!bbWorkSet.empty()) {
                    BasicBlock* blk = *(bbWorkSet.begin()); // Gets first element in the set.
                    bbWorkSet.erase(blk); // Remove used block from the set.

                    InstrSet genSet = computeBBGenSet(blk);
                    InstrSet killSet = computeBBKillSet(blk);

                    InstrSet liveAfter = computeBBLiveAfterSet(blk, bbLiveBefore);

                    // Assign new live after set to basic block.
                    delete bbLiveAfter[blk];
                    bbLiveAfter[blk] = liveAfter;

                    // Compute new live before set using new live after, gen and kill sets.
                    InstrSet liveBefore = bbLiveBefore[blk];
                    InstrSet newLiveBefore = computeNewLiveBefore(liveAfter, genSet, killSet);

                    if (!(*liveBefore == *newLiveBefore)) {
                        delete liveBefore;
                        bbLiveBefore[blk] = newLiveBefore;
                        for (pred_iterator pit = pred_begin(blk); pit != pred_end(blk); pit++) {
                            bbWorkSet.insert(*pit);
                        }
                    } else {
                        delete newLiveBefore;
                    }

                    delete genSet;
                    delete killSet;
                }

                // Compute live before/after sets for each instruction in the block. Account for dead instructions.

                computeInstructionLiveSets(F, bbLiveBefore, bbLiveAfter, instLiveBefore, instLiveAfter);

                // Iterate over all instructions and collect dead instructions.
                InstrSet deadInstrs = new std::set<Instruction*>();
                for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                    Instruction* instr = &*I;
                    if (isDeadInstruction(instr) && !instLiveAfter[instr]->count(instr)) {
                        changed = true;
                        // Add dead instruction for removal.
                        deadInstrs->insert(instr);
                    }
                }

                // Remove dead instruction
                for (InstrIter it = deadInstrs->begin(); it != deadInstrs->end(); ++it) {
                    (*it)->eraseFromParent();
                }

                if (!changed) break;

            }

            instructionsCount(F);

            errs() << "DCE END\n";
            return false;
        }

        virtual void computeInstructionLiveSets(Function &F,
                                                std::map<BasicBlock*, InstrSet> &bbLiveBefore,
                                                std::map<BasicBlock*, InstrSet> &bbLiveAfter,
                                                std::map<Instruction*, InstrSet> &instLiveBefore,
                                                std::map<Instruction*, InstrSet> &instLiveAfter) {
            Function::iterator fit;
            BasicBlock::iterator bit;
            for (fit = F.begin(); fit != F.end(); ++fit) {
                // Iterate all instructions in the block backwards.
                BasicBlock* blk = &*fit;
                bit = blk->end();

                Instruction* current, *next, *lastSuccessful;
                bool terminator = true; // Last instruction in the block is terminator instruction.
                do {
                    --bit;
                    next = current;
                    current = &*bit;

                    instLiveAfter[current] = new std::set<Instruction*>(); // Prepare instruction live after set.

                    if (terminator) {
                        // Terminator instruction live after set is the same block live after set.
                        *(instLiveAfter[current]) = *(bbLiveAfter[blk]);
                        lastSuccessful = current;

                        terminator = false;
                    } else {
                        // Otherwise, copy live before of successor instruction.
                        *(instLiveAfter[current]) = *(instLiveBefore[next]);
                    }

                    // Instruction is dead if it's live after set does not contain it and it is one of the
                    // possible instruction without side effects.

                    if (!instLiveAfter[current]->count(current) && isDeadInstruction(current)) {
                        // In this case, copy last successful instruction liveBefore set to
                        // current instruction's liveBefore set.
                        instLiveBefore[current] = instLiveBefore[lastSuccessful];
                    } else {
                        // Otherwise, instruction is live, so just use data flow equation
                        // to compute instruction's live before set.

                        // Also, note that this is not most recent successful live instruction.
                        lastSuccessful = current;

                        instLiveBefore[current] = computeInstructionLiveBefore(current, instLiveAfter);
                    }
                } while (bit != blk->begin());
            }
        }

        virtual InstrSet computeInstructionLiveBefore(Instruction* I,
                                                      std::map<Instruction*, InstrSet> &instLiveAfter) {
            // Start with empty set. Add all instructions of live after set. Then delete kill set
            // instruction. Finally add all operands of instruction that are instructions.

            InstrSet instrLiveBefore = new std::set<Instruction*>();

            *instrLiveBefore = *(instLiveAfter[I]);

            instrLiveBefore->erase(I); // Delete kill set.

            // Add all instruction's operands that are instructions. Gen set.
            User::op_iterator opit;
            for (opit = I->op_begin(); opit != I->op_end(); opit++)
            {
                Use *use = &*opit;
                Value *usedValue = use->get();
                if (isa<Instruction>(usedValue)) {
                    Instruction *usedInstr = cast<Instruction>(usedValue);
                    if (usedInstr != NULL) {
                        instrLiveBefore->insert(usedInstr);
                    }
                }
            }

            return instrLiveBefore;
        }

        virtual bool isDeadInstruction(Instruction* I) {
            return I->isBinaryOp() ||
                   I->isCast() ||
                   I->isShift() ||
                   I->getOpcode() == Instruction::Alloca ||
                   I->getOpcode() == Instruction::Load ||
                   I->getOpcode() == Instruction::GetElementPtr ||
                   I->getOpcode() == Instruction::Select ||
                   I->getOpcode() == Instruction::ExtractElement ||
                   I->getOpcode() == Instruction::ExtractValue ||
                   I->getOpcode() == Instruction::ICmp ||
                   I->getOpcode() == Instruction::FCmp;
        }

        virtual InstrSet computeNewLiveBefore(InstrSet liveAfter,
                                              InstrSet genSet,
                                              InstrSet killSet) {
            InstrSet newLiveBefore = new std::set<Instruction*>();

            // LiveBefore = genSet UNION (liveAfter DIFFERENCE killSet)

            // Firstly, insert liveAfter into newLiveBefore
            for (InstrIter it = liveAfter->begin(); it != liveAfter->end(); ++it) {
                newLiveBefore->insert(*it);
            }

            // Then remove all elements from liveAfter that are in killSet aka set difference.
            for (InstrIter it = killSet->begin(); it != killSet->end(); ++it) {
                newLiveBefore->erase(*it);
            }

            // Add all items from genSet into newLiveBefore set.
            for (InstrIter it = genSet->begin(); it != genSet->end(); ++it) {
                newLiveBefore->insert(*it);
            }

            return newLiveBefore;

        }

        virtual InstrSet computeBBKillSet(BasicBlock* blk) {
            // The kill set of basic block is a combination of all kill sets for each instruction.
            // The kill set for instruction is an instruction itself.
            InstrSet killSet = new std::set<Instruction*>();

            BasicBlock::iterator bit;
            for (bit = blk->begin(); bit != blk->end(); ++bit) {
                killSet->insert(&*bit);
            }
            return killSet;
        }

        virtual InstrSet computeBBGenSet(BasicBlock* blk) {
            // The gen set for the basic block are all unique uses of instruction
            // in each instruction.
            InstrSet genSet = new std::set<Instruction*>();

            BasicBlock::iterator bit;

            InstrSet seen = new std::set<Instruction*>();

            for (bit = blk->begin(); bit != blk->end(); ++bit) {
                Instruction* I = &*bit;

                User::op_iterator opit;
                for (opit = I->op_begin(); opit != I->op_end(); opit++) {
                    Use* use = &*opit;
                    Value* usedValue = use->get();
                    if (isa<Instruction>(usedValue)) {
                        // The Value is an Instruction.
                        Instruction* usedInstr = cast<Instruction>(usedValue);

                        if (!seen->count(usedInstr)) {
                            genSet->insert(usedInstr);
                        }
                    }
                }
                seen->insert(I);
            }

            delete seen;

            return genSet;
        }

        virtual InstrSet computeBBLiveAfterSet(BasicBlock* blk,
                                               std::map<BasicBlock*, InstrSet> &bbLiveBefore) {
            InstrSet liveAfter = new std::set<Instruction*>();
            for (succ_iterator s_it = succ_begin(blk); s_it != succ_end(blk); s_it++) {
                InstrSet basicBlockLiveBefore = bbLiveBefore[*s_it];
                std::set<Instruction*>::iterator it;
                for (InstrIter it = basicBlockLiveBefore->begin(); it != basicBlockLiveBefore->end(); it++) {
                    liveAfter->insert(*it);
                }
            }
            return liveAfter;
        }
    };
}
char MyDCE::ID = 0;
>>>>>>> 495f456b4d4a00cf2701b1796cce8b8d0a0958fc
static RegisterPass<MyDCE> X("mydce", "My dead code elimination");