#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

bool promote(IRBuilder *builder, Value **l_val_p, Value **r_val_p) {
    bool is_int = false;
    auto &l_val = *l_val_p;
    auto &r_val = *r_val_p;
    if (l_val->get_type() == r_val->get_type()) {
        is_int = l_val->get_type()->is_integer_type();
    } else {
        if (l_val->get_type()->is_integer_type()) {
            l_val = builder->create_sitofp(l_val, FLOAT_T);
        } else {
            r_val = builder->create_sitofp(r_val, FLOAT_T);
        }
    }
    return is_int;
}

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

Value* CminusfBuilder::visit(ASTProgram &node) {
    VOID_T = module->get_void_type();
    INT1_T = module->get_int1_type();
    INT32_T = module->get_int32_type();
    INT32PTR_T = module->get_int32_ptr_type();
    FLOAT_T = module->get_float_type();
    FLOATPTR_T = module->get_float_ptr_type();

    Value *ret_val = nullptr;
    for (auto &decl : node.declarations) {
        ret_val = decl->accept(*this);
    }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTNum &node) {
    if (node.type == TYPE_INT) {
        return CONST_INT(node.i_val);
    }
    return CONST_FP(node.f_val);
}

Value* CminusfBuilder::visit(ASTVarDeclaration &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if (node.num == nullptr)//非数组类型
    {
        if(node.type == TYPE_INT) {
          //这里push进去的到底是啥？？
            if(context.func == nullptr) {
                auto initializer = ConstantZero::get(INT32_T, module.get());
                auto ialloca = GlobalVariable::create(node.id, module.get(), INT32_T, false, initializer);
                scope.push(node.id, ialloca);
            }
            else {
                auto ialloca = builder->create_alloca(INT32_T);
                scope.push(node.id, ialloca); //这里push进去的到底是啥？？
            }
        }
        else {
            if(context.func == nullptr) {
                auto initializer = ConstantZero::get(FLOAT_T, module.get());
                auto falloca = GlobalVariable::create(node.id, module.get(), FLOAT_T, false, initializer);
                scope.push(node.id, falloca);
            }
            else {
                auto falloca = builder->create_alloca(FLOAT_T);
                scope.push(node.id, falloca);
            }
        }
    }
    else {  //数组类型
        if(node.type == TYPE_INT) {
            if(context.func == nullptr) {   //全局变量
                auto *arrayType = ArrayType::get(INT32_T, node.num->i_val);
                auto initializer = ConstantZero::get(INT32_T, module.get());
                auto inalloca = GlobalVariable::create(node.id, module.get(), arrayType, false, initializer);
                scope.push(node.id, inalloca);
            }
            else {  
                auto *arrayType = ArrayType::get(INT32_T, node.num->i_val);
                auto inalloca = builder->create_alloca(arrayType);
                scope.push(node.id, inalloca);
            }
        }
        else {
            if(context.func == nullptr) {
                auto *arrayType = ArrayType::get(FLOAT_T, node.num->i_val);
                auto initializer = ConstantZero::get(FLOAT_T, module.get());
                auto fnalloca = GlobalVariable::create(node.id, module.get(), arrayType, false, initializer);
                scope.push(node.id, fnalloca);
            }
            else {
                auto *arrayType = ArrayType::get(FLOAT_T, node.num->i_val);
                auto fnalloca = builder->create_alloca(arrayType);
                scope.push(node.id, fnalloca);
            }
        }
    }
    
    return nullptr;
}

Value* CminusfBuilder::visit(ASTFunDeclaration &node) {
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params) {
        if (param->type == TYPE_INT) {
            if (param->isarray) {
                param_types.push_back(INT32PTR_T);
            } else {
                param_types.push_back(INT32_T);
            }
        } else {
            if (param->isarray) {
                param_types.push_back(FLOATPTR_T);
            } else {
                param_types.push_back(FLOAT_T);
            }
        }
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto func = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, func);
    context.func = func;
    auto funBB = BasicBlock::create(module.get(), "entry", func);
    builder->set_insert_point(funBB);
    scope.enter();
    // context.pre_enter_scope = true;
    std::vector<Value *> args;
    for (auto &arg : func->get_args()) {
        args.push_back(&arg);
    }
    for (unsigned int i = 0; i < node.params.size(); ++i) {
        // TODO: You need to deal with params and store them in the scope.
        node.params[i]->accept(*this);
        auto addr = scope.find(node.params[i]->id);
        builder->create_store(args[i], addr);
    }
    node.compound_stmt->accept(*this);
    if (!builder->get_insert_block()->is_terminated())
    {
        if (context.func->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (context.func->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTParam &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.isarray) {      //是数组
        if(node.type == TYPE_INT) {
            
            auto inAlloca = builder->create_alloca(INT32PTR_T);
            scope.push(node.id, inAlloca);
        }
        else {
            auto fnAlloca = builder->create_alloca(FLOATPTR_T);
            scope.push(node.id, fnAlloca);
        }
        
    }
    else {
        if(node.type == TYPE_INT) {
            auto iAlloca = builder->create_alloca(INT32_T);
            scope.push(node.id, iAlloca);
        }
        else {
            auto fAlloca = builder->create_alloca(FLOAT_T);
            scope.push(node.id, fAlloca);
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCompoundStmt &node) {
    // TODO: This function is not complete.
    // You may need to add some code here
    // to deal with complex statements.
    scope.enter();
    for (auto &decl : node.local_declarations) {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list) {
        stmt->accept(*this);
        if (builder->get_insert_block()->is_terminated())
            break;
    }
//    builder->create_void_ret();
    scope.exit();
    return nullptr;
}

Value* CminusfBuilder::visit(ASTExpressionStmt &node) {
    if (node.expression != nullptr) {
        return node.expression->accept(*this);
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTSelectionStmt &node) {
    auto *ret_val = node.expression->accept(*this);
    auto *trueBB = BasicBlock::create(module.get(), "", context.func);
    BasicBlock *falseBB{};
    auto *contBB = BasicBlock::create(module.get(), "", context.func);
    Value *cond_val = nullptr;
    if (ret_val->get_type()->is_integer_type()) {
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    } else {
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));
        }

    if (node.else_statement == nullptr) {
        builder->create_cond_br(cond_val, trueBB, contBB);
    } else {
        falseBB = BasicBlock::create(module.get(), "", context.func);
        builder->create_cond_br(cond_val, trueBB, falseBB);
    }
        builder->set_insert_point(trueBB);
        node.if_statement->accept(*this);

    if (not builder->get_insert_block()->is_terminated()) {
        builder->create_br(contBB);
    }

    if (node.else_statement == nullptr) {
        // falseBB->erase_from_parent(); // did not clean up memory
    } else {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (not builder->get_insert_block()->is_terminated()) {
            builder->create_br(contBB);
        }
    }

    builder->set_insert_point(contBB);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTIterationStmt &node) {
    // TODO: This function is empty now.
    // Add some code here.
    auto relationBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    auto relopBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    auto retBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
    
    //auto originBB = context.now_bb;
    builder->create_br(relationBB);

    builder->set_insert_point(relationBB);
    //context.now_bb = relationBB;
    auto cond = node.expression->accept(*this);
    Value* trans_cond;
    if(cond->get_type() == INT32_T) {
        trans_cond = builder->create_icmp_gt(cond, CONST_INT(0));
    }
    else if(cond->get_type() == FLOAT_T) {
        trans_cond = builder->create_fcmp_gt(cond, CONST_FP(0));
    }
//    auto cond_int = static_cast<int>(cond);
    builder->create_cond_br(trans_cond, relopBB, retBB);

    builder->set_insert_point(relopBB);
    //context.now_bb = relopBB;
    node.statement->accept(*this);
    if(!builder->get_insert_block()->is_terminated()) {
        builder->create_br(relationBB);
    }
    builder->set_insert_point(retBB);
//    builder->create_ret(a);
    //context.now_bb = originBB;
//    builder->create_br(context.now_bb);
    return nullptr;
}

Value* CminusfBuilder::visit(ASTReturnStmt &node) {
    if (node.expression == nullptr) {
        builder->create_void_ret();
    } else {
        auto *fun_ret_type =
            context.func->get_function_type()->get_return_type();
        auto *ret_val = node.expression->accept(*this);
        if (fun_ret_type != ret_val->get_type()) {
            if (fun_ret_type->is_integer_type()) {
                ret_val = builder->create_fptosi(ret_val, INT32_T);
            } else {
                ret_val = builder->create_sitofp(ret_val, FLOAT_T);
                }
            }   

        builder->create_ret(ret_val);
        }

    return nullptr;
}

Value* CminusfBuilder::visit(ASTVar &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.expression == nullptr) {//非数组元素
        auto addr = scope.find(node.id);
        //return builder->create_load(addr);
       /* if(addr->get_type()->is_array_type()) //若某函数调用的实参是数组名称
            return &addr;*/
        return addr;
    }
    else {//数组元素
        auto idx = node.expression->accept(*this);
        ICmpInst *icmp;
        if(idx->get_type() == FLOAT_T) {
            auto idx_trans = builder->create_fptosi(idx, INT32_T);
            icmp = builder->create_icmp_ge(idx_trans, CONST_INT(0));
        }
        else
            icmp = builder->create_icmp_ge(idx, CONST_INT(0));

        auto newBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        auto trueBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        auto falseBB = BasicBlock::create(module.get(), context.label.append("b"), context.func);
        builder->create_cond_br(icmp, trueBB, falseBB);
        builder->set_insert_point(trueBB);
        builder->create_br(newBB);
        builder->set_insert_point(falseBB);
        builder->create_call(scope.find("neg_idx_except"),{} );
        builder->create_br(newBB);

        builder->set_insert_point(newBB);
        //context.now_bb = newBB;
        auto addr = scope.find(node.id);
        std::cout << addr->get_type()->print() << std::endl;
        if(idx->get_type() == FLOAT_T) {
            if(addr->get_type()->get_pointer_element_type() == INT32PTR_T ||
            addr->get_type()->get_pointer_element_type() == FLOATPTR_T) {//函数参数是数组时
                auto idx_trans = builder->create_fptosi(idx, INT32_T);
                auto addr_trans = builder->create_load(addr);   //将参数转化为数组类型
                auto ADDR = builder->create_gep(addr_trans, {/*CONST_FP(0), */idx_trans});
                return ADDR;
            }   
            auto idx_trans = builder->create_fptosi(idx, INT32_T);
            auto ADDR = builder->create_gep(addr, {CONST_INT(0),idx_trans});
            //std::cout << ADDR->print() <<std::endl;
            //return builder->create_load(ADDR);
            return ADDR;
        }
        
        if(addr->get_type()->get_pointer_element_type() == INT32PTR_T ||
            addr->get_type()->get_pointer_element_type() == FLOATPTR_T) {//addr不是数组类型，也就是当var是函数数组类型形参时
            auto addr_trans = builder->create_load(addr);   //将参数转化为数组类型
            //builder->create_alloca
            auto ADDR = builder->create_gep(addr_trans, {/*CONST_INT(0), */idx});
            return ADDR;
        }
        auto ADDR = builder->create_gep(addr, {CONST_INT(0), idx});
       // std::cout << ADDR->print() <<std::endl;
        //return builder->create_load(ADDR);
        return ADDR;
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAssignExpression &node) {
    auto *expr_result = node.expression->accept(*this);
    context.require_lvalue = true;
    auto *var_addr = node.var->accept(*this);
    if (var_addr->get_type()->get_pointer_element_type() !=
        expr_result->get_type()) {
        if (expr_result->get_type() == INT32_T) {
            expr_result = builder->create_sitofp(expr_result, FLOAT_T);
        } else {
            expr_result = builder->create_fptosi(expr_result, INT32_T);
        }
    }
    builder->create_store(expr_result, var_addr);
    return expr_result;
}

Value* CminusfBuilder::visit(ASTSimpleExpression &node) {
    // TODO: This function is empty now.
    // Add some code here.
    if(node.additive_expression_r == nullptr) {
        return node.additive_expression_l->accept(*this);
    }
    else {
        auto ret1 = node.additive_expression_l->accept(*this);
        auto ret2 = node.additive_expression_r->accept(*this);
        if(ret1->get_type() != FLOAT_T && ret2->get_type() != FLOAT_T) {
            ICmpInst *icmp;
            switch (node.op)
            {
            case OP_LE: 
                icmp = builder->create_icmp_le(ret1, ret2);
                break;
            case OP_LT:
                icmp = builder->create_icmp_lt(ret1, ret2);
                break;
            case OP_GT:
                icmp = builder->create_icmp_gt(ret1, ret2);
                break;
            case OP_GE:
                icmp = builder->create_icmp_ge(ret1, ret2);
                break;
            case OP_EQ:
                icmp = builder->create_icmp_eq(ret1, ret2);
                break;
            case OP_NEQ:
                icmp = builder->create_icmp_ne(ret1, ret2);
                break;
            default:
                break;
            }
            auto icmp_ret = builder->create_zext(icmp, INT32_T);
            return icmp_ret;
        }
        else if(ret1->get_type() == FLOAT_T && ret2->get_type() == FLOAT_T){
            FCmpInst* fcmp;
            switch (node.op)
            {
            case OP_LE: 
                fcmp = builder->create_fcmp_le(ret1, ret2);
                break;
            case OP_LT:
                fcmp = builder->create_fcmp_lt(ret1, ret2);
                break;
            case OP_GT:
                fcmp = builder->create_fcmp_gt(ret1, ret2);
                break;
            case OP_GE:
                fcmp = builder->create_fcmp_ge(ret1, ret2);
                break;
            case OP_EQ:
                fcmp = builder->create_fcmp_eq(ret1, ret2);
                break;
            case OP_NEQ:
                fcmp = builder->create_fcmp_ne(ret1, ret2);
                break;
            default:
                break;
            }
            auto fcmp_ret = builder->create_zext(fcmp, INT32_T);
            return fcmp_ret;
        }
        else {
            FCmpInst* fcmp;
            if(ret1->get_type() != FLOAT_T) {
                auto ret1_trans = builder->create_sitofp(ret1,FLOAT_T);
                switch (node.op)
            {
            case OP_LE: 
                fcmp = builder->create_fcmp_le(ret1_trans, ret2);
                break;
            case OP_LT:
                fcmp = builder->create_fcmp_lt(ret1_trans, ret2);
                break;
            case OP_GT:
                fcmp = builder->create_fcmp_gt(ret1_trans, ret2);
                break;
            case OP_GE:
                fcmp = builder->create_fcmp_ge(ret1_trans, ret2);
                break;
            case OP_EQ:
                fcmp = builder->create_fcmp_eq(ret1_trans, ret2);
                break;
            case OP_NEQ:
                fcmp = builder->create_fcmp_ne(ret1_trans, ret2);
                break;
            default:
                break;
            }
            auto fcmp_ret = builder->create_zext(fcmp, INT32_T);
            return fcmp_ret;
            }
            else {  //ret2为整形
                auto re2_trans = builder->create_sitofp(ret2,FLOAT_T);
                switch (node.op)
            {
            case OP_LE: 
                fcmp = builder->create_fcmp_le(ret1, re2_trans);
                break;
            case OP_LT:
                fcmp = builder->create_fcmp_lt(ret1, re2_trans);
                break;
            case OP_GT:
                fcmp = builder->create_fcmp_gt(ret1, re2_trans);
                break;
            case OP_GE:
                fcmp = builder->create_fcmp_ge(ret1, re2_trans);
                break;
            case OP_EQ:
                fcmp = builder->create_fcmp_eq(ret1, re2_trans);
                break;
            case OP_NEQ:
                fcmp = builder->create_fcmp_ne(ret1, re2_trans);
                break;
            default:
                break;
            }
            auto fcmp_ret = builder->create_zext(fcmp, INT32_T);
            return fcmp_ret;
            }
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTAdditiveExpression &node) {
    if (node.additive_expression == nullptr) {
        return node.term->accept(*this);
    }

    auto *l_val = node.additive_expression->accept(*this);
    auto *r_val = node.term->accept(*this);
    bool is_int = promote(&*builder, &l_val, &r_val);
    Value *ret_val = nullptr;
    switch (node.op) {
    case OP_PLUS:
        if (is_int) {
            ret_val = builder->create_iadd(l_val, r_val);
        } else {
            ret_val = builder->create_fadd(l_val, r_val);
            }
            break;
        case OP_MINUS:
        if (is_int) {
            ret_val = builder->create_isub(l_val, r_val);
        } else {
            ret_val = builder->create_fsub(l_val, r_val);
                }
            break;
        }
    return ret_val;
}

Value* CminusfBuilder::visit(ASTTerm &node) {

    auto op2 = node.factor->accept(*this);
    if(node.term == nullptr) {
        if(op2->get_type()->is_pointer_type()) {
            if(op2->get_type()->get_pointer_element_type()->is_array_type())  {
            //f(a),a是数组名，且a为局部变量
                return builder->create_gep(op2, {CONST_INT(0), CONST_INT(0)});
            }
            else if(op2->get_type()->get_pointer_element_type()->is_pointer_type()) {
            //f(a),a是数组名，且a为形参
                return builder->create_load(op2);
            }
        }
        
        if(op2->get_type() == INT32PTR_T || op2->get_type() == FLOATPTR_T) { 
            return builder->create_load(op2);
        }
        return op2;
    }
    else {//有两个操作数的运算表达式
        auto op1 = node.term->accept(*this);
        if(op2->get_type()->is_pointer_type()) {
            if(op2->get_type()->get_pointer_element_type()->is_array_type())  {
            //f(a),a是数组名，且a为局部变量
                op2 = builder->create_gep(op2, {CONST_INT(0), CONST_INT(0)});
            }
            else if(op2->get_type()->get_pointer_element_type()->is_pointer_type()) {
            //f(a),a是数组名，且a为形参
                op2= builder->create_load(op2);
            }
        }
        if(op2->get_type() == INT32PTR_T || op2->get_type() == FLOATPTR_T) {
            //不是数组
            op2 = builder->create_load(op2);
        }

        switch (node.op)
        {
        case OP_MUL:
            if(op1->get_type() == INT32_T && op2->get_type() == INT32_T)
                return builder->create_imul(op1, op2);
            else if(op1->get_type() == FLOAT_T && op2->get_type() == FLOAT_T)
                return builder->create_fmul(op1, op2);
            else {
                if(op1->get_type() == INT32_T) {
                    auto trans_op1 = builder->create_sitofp(op1, FLOAT_T);
                    return builder->create_fmul(trans_op1, op2);
                }
                if(op2->get_type() == INT32_T) {
                    auto trans_op2 = builder->create_sitofp(op2, FLOAT_T);
                    return builder->create_fmul(op1, trans_op2);
                }
                
            }
            break;
        case OP_DIV:
            if(op1->get_type() == INT32_T && op2->get_type() == INT32_T)
                return builder->create_isdiv(op1, op2);
            else if(op1->get_type() == FLOAT_T && op2->get_type() == FLOAT_T)
                return builder->create_fdiv(op1, op2);
            else {
                if(op1->get_type() == INT32_T) {
                    auto trans_op1 = builder->create_sitofp(op1, FLOAT_T);
                    return builder->create_fdiv(trans_op1, op2);
                }
                if(op2->get_type() == INT32_T) {
                    auto trans_op2 = builder->create_sitofp(op2, FLOAT_T);
                    return builder->create_fdiv(op1, trans_op2);
                }
                
            }
            break;
        default:
            break;
        }
    }
    return nullptr;
}

Value* CminusfBuilder::visit(ASTCall &node) {
    auto *func = dynamic_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    auto param_type = func->get_function_type()->param_begin();
    for (auto &arg : node.args) {
        auto *arg_val = arg->accept(*this);
        if (!arg_val->get_type()->is_pointer_type() &&
            *param_type != arg_val->get_type()) {
            if (arg_val->get_type()->is_integer_type()) {
                arg_val = builder->create_sitofp(arg_val, FLOAT_T);
            } else {
                arg_val = builder->create_fptosi(arg_val, INT32_T);
            }
        }
        args.push_back(arg_val);
        param_type++;
    }

    return builder->create_call(static_cast<Function *>(func), args);
}
