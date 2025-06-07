#include "DeadCode.hpp"
#include "logging.hpp"
#include <vector>
#include <unordered_set>

void DeadCode::run() {
    bool changed = false;
    func_info->run();
    do {
        changed = false;
        for (auto &F : m_->get_functions()) {
            auto *func = &F;
            changed |= clear_basic_blocks(func);
            mark(func);
            changed |= sweep(func);
        }
        sweep_globally();
    } while (changed);
    LOG_INFO << "dead code pass erased " << ins_count << " instructions";
}

bool DeadCode::clear_basic_blocks(Function *func) {
    bool changed = false;
    std::vector<BasicBlock *> to_erase;
    for (auto &bb : func->get_basic_blocks()) {
        if (bb.get_pre_basic_blocks().empty() && &bb != func->get_entry_block()) {
            to_erase.push_back(&bb);
            changed = true;
        }
    }
    for (auto *bb : to_erase) {
        bb->erase_from_parent();
    }
    return changed;
}

void DeadCode::mark(Function *function) {
    marked.clear();
    work_list.clear();

    // 初始标记关键指令并加入工作队列
    for (auto &block : function->get_basic_blocks()) {
        auto &instructions = block.get_instructions();
        for (auto &inst : instructions) {
            if (is_critical(&inst)) {
                marked.emplace(&inst, true);
                work_list.emplace_back(&inst);
            }
        }
    }

    // 使用工作队列传播标记
    while (!work_list.empty()) {
        Instruction *currentInst = work_list.front();
        work_list.pop_front();
        mark(currentInst);
    }
}


void DeadCode::mark(Instruction *ins) {
    for (auto *op : ins->get_operands()) {
        auto *def = dynamic_cast<Instruction *>(op);
        if (!def)
            continue;
        if (def->get_function() != ins->get_function())
            continue;
        if (marked.count(def))
            continue;
        marked[def] = true;
        work_list.push_back(def);
    }
}

bool DeadCode::is_critical(Instruction *ins) {
    const bool isExitOrControl = ins->is_ret() || ins->is_br();
    const bool isMemWriteOrPhi = ins->is_store() || ins->is_phi();
    
    if (isExitOrControl || isMemWriteOrPhi) {
        return true;
    }
    
    if (ins->is_call()) {
        Value *callTarget = ins->get_operand(0);
        Function *callee = dynamic_cast<Function *>(callTarget);
        
        if (!callee) {
            return true;
        }
        
        bool hasSideEffects = !func_info->is_pure_function(callee);
        return hasSideEffects;
    }
    
    const bool isReferenced = ins->get_use_list().size() > 0;
    return isReferenced;
}

bool DeadCode::sweep(Function *func) {
    std::vector<Instruction *> instructionsToDelete;

    for (auto &basicBlock : func->get_basic_blocks()) {
        for (auto &instruction : basicBlock.get_instructions()) {
            if (marked.find(&instruction) == marked.end()) {
                instructionsToDelete.push_back(&instruction);
            }
        }
    }

    for (auto *instr : instructionsToDelete) {
        size_t operandCount = instr->get_num_operand();
        for (size_t idx = 0; idx < operandCount; ++idx) {
            if (auto *operand = instr->get_operand(idx)) {
                operand->remove_use(instr, idx);
            }
        }

        auto *parentBlock = instr->get_parent();
        if (parentBlock) {
            parentBlock->remove_instr(instr);
        }

        ++ins_count;
    }

    return !instructionsToDelete.empty();
}


void DeadCode::sweep_globally() {
    std::vector<Function *> funcs_to_remove;
    std::vector<GlobalVariable *> globals_to_remove;

    for (auto it = m_->get_functions().rbegin(); it != m_->get_functions().rend(); ++it) {
        if (it->get_name() != "main" && it->get_use_list().size() == 0) {
            funcs_to_remove.push_back(&*it);
        }
    }

    auto& globals = m_->get_global_variable();
    for (int idx = 0; idx < globals.size(); ++idx) {
        auto it = globals.begin();
        std::advance(it, idx);
        if (!it->get_use_list().empty()) continue;
        globals_to_remove.push_back(&*it);
    }

    for (auto rit = globals_to_remove.rbegin(); rit != globals_to_remove.rend(); ++rit) {
        if (*rit) globals.erase((*rit)->getIterator());
    }

    for (Function* func_ptr : funcs_to_remove) {
        if (func_ptr) m_->get_functions().erase(func_ptr->getIterator());
    }
}