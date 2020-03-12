/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/HashMap.h>
#include <AK/StringBuilder.h>
#include <LibJS/AST.h>
#include <LibJS/Function.h>
#include <LibJS/Interpreter.h>
#include <LibJS/PrimitiveString.h>
#include <LibJS/Value.h>
#include <stdio.h>

namespace JS {

Value ScopeNode::execute(Interpreter& interpreter) const
{
    return interpreter.run(*this);
}

Value FunctionDeclaration::execute(Interpreter& interpreter) const
{
    auto* function = interpreter.heap().allocate<Function>(name(), body(), parameters());
    interpreter.set_variable(m_name, Value(function));
    return Value(function);
}

Value ExpressionStatement::execute(Interpreter& interpreter) const
{
    return m_expression->execute(interpreter);
}

Value CallExpression::execute(Interpreter& interpreter) const
{
    if (name() == "$gc") {
        interpreter.heap().collect_garbage();
        return js_undefined();
    }

    auto callee = interpreter.get_variable(name());
    ASSERT(callee.is_object());
    auto* callee_object = callee.as_object();
    ASSERT(callee_object->is_function());
    auto& function = static_cast<Function&>(*callee_object);

    const size_t arguments_size = m_arguments.size();
    ASSERT(function.parameters().size() == arguments_size);
    HashMap<String, Value> passed_parameters;
    for (size_t i = 0; i < arguments_size; ++i) {
        auto name = function.parameters()[i];
        auto value = m_arguments[i].execute(interpreter);
        dbg() << name << ": " << value;
        passed_parameters.set(move(name), move(value));
    }

    return interpreter.run(function.body(), move(passed_parameters), ScopeType::Function);
}

Value ReturnStatement::execute(Interpreter& interpreter) const
{
    auto value = argument() ? argument()->execute(interpreter) : js_undefined();
    interpreter.do_return();
    return value;
}

Value IfStatement::execute(Interpreter& interpreter) const
{
    auto predicate_result = m_predicate->execute(interpreter);

    if (predicate_result.to_boolean())
        return interpreter.run(*m_consequent);
    else
        return interpreter.run(*m_alternate);
}

Value WhileStatement::execute(Interpreter& interpreter) const
{
    Value last_value = js_undefined();
    while (m_predicate->execute(interpreter).to_boolean()) {
        last_value = interpreter.run(*m_body);
    }

    return last_value;
}

Value BinaryExpression::execute(Interpreter& interpreter) const
{
    auto lhs_result = m_lhs->execute(interpreter);
    auto rhs_result = m_rhs->execute(interpreter);

    switch (m_op) {
    case BinaryOp::Plus:
        return add(lhs_result, rhs_result);
    case BinaryOp::Minus:
        return sub(lhs_result, rhs_result);
    case BinaryOp::TypedEquals:
        return typed_eq(lhs_result, rhs_result);
    case BinaryOp::TypedInequals:
        return Value(!typed_eq(lhs_result, rhs_result).to_boolean());
    case BinaryOp::GreaterThan:
        return greater_than(lhs_result, rhs_result);
    case BinaryOp::LessThan:
        return less_than(lhs_result, rhs_result);
    case BinaryOp::BitwiseAnd:
        return bitwise_and(lhs_result, rhs_result);
    case BinaryOp::BitwiseOr:
        return bitwise_or(lhs_result, rhs_result);
    case BinaryOp::BitwiseXor:
        return bitwise_xor(lhs_result, rhs_result);
    case BinaryOp::LeftShift:
        return left_shift(lhs_result, rhs_result);
    case BinaryOp::RightShift:
        return right_shift(lhs_result, rhs_result);
    }

    ASSERT_NOT_REACHED();
}

Value LogicalExpression::execute(Interpreter& interpreter) const
{
    auto lhs_result = m_lhs->execute(interpreter).to_boolean();
    auto rhs_result = m_rhs->execute(interpreter).to_boolean();
    switch (m_op) {
    case LogicalOp::And:
        return Value(lhs_result && rhs_result);
    case LogicalOp::Or:
        return Value(lhs_result || rhs_result);
    }

    ASSERT_NOT_REACHED();
}

Value UnaryExpression::execute(Interpreter& interpreter) const
{
    auto lhs_result = m_lhs->execute(interpreter);
    switch (m_op) {
    case UnaryOp::BitNot:
        return bitwise_not(lhs_result);
    case UnaryOp::Not:
        return Value(!lhs_result.to_boolean());
    }

    ASSERT_NOT_REACHED();
}

static void print_indent(int indent)
{
    for (int i = 0; i < indent * 2; ++i)
        putchar(' ');
}

void ASTNode::dump(int indent) const
{
    print_indent(indent);
    printf("%s\n", class_name());
}

void ScopeNode::dump(int indent) const
{
    ASTNode::dump(indent);
    for (auto& child : children())
        child.dump(indent + 1);
}

void BinaryExpression::dump(int indent) const
{
    const char* op_string = nullptr;
    switch (m_op) {
    case BinaryOp::Plus:
        op_string = "+";
        break;
    case BinaryOp::Minus:
        op_string = "-";
        break;
    case BinaryOp::TypedEquals:
        op_string = "===";
        break;
    case BinaryOp::TypedInequals:
        op_string = "!==";
        break;
    case BinaryOp::GreaterThan:
        op_string = ">";
        break;
    case BinaryOp::LessThan:
        op_string = "<";
        break;
    case BinaryOp::BitwiseAnd:
        op_string = "&";
        break;
    case BinaryOp::BitwiseOr:
        op_string = "|";
        break;
    case BinaryOp::BitwiseXor:
        op_string = "^";
        break;
    case BinaryOp::LeftShift:
        op_string = "<<";
        break;
    case BinaryOp::RightShift:
        op_string = ">>";
        break;
    }

    print_indent(indent);
    printf("%s\n", class_name());
    m_lhs->dump(indent + 1);
    print_indent(indent + 1);
    printf("%s\n", op_string);
    m_rhs->dump(indent + 1);
}

void LogicalExpression::dump(int indent) const
{
    const char* op_string = nullptr;
    switch (m_op) {
    case LogicalOp::And:
        op_string = "&&";
        break;
    case LogicalOp::Or:
        op_string = "||";
        break;
    }

    print_indent(indent);
    printf("%s\n", class_name());
    m_lhs->dump(indent + 1);
    print_indent(indent + 1);
    printf("%s\n", op_string);
    m_rhs->dump(indent + 1);
}

void UnaryExpression::dump(int indent) const
{
    const char* op_string = nullptr;
    switch (m_op) {
    case UnaryOp::BitNot:
        op_string = "~";
        break;
    case UnaryOp::Not:
        op_string = "!";
        break;
    }

    print_indent(indent);
    printf("%s\n", class_name());
    print_indent(indent + 1);
    printf("%s\n", op_string);
    m_lhs->dump(indent + 1);
}

void CallExpression::dump(int indent) const
{
    print_indent(indent);
    printf("%s '%s'\n", class_name(), name().characters());
}

void StringLiteral::dump(int indent) const
{
    print_indent(indent);
    printf("StringLiteral \"%s\"\n", m_value.characters());
}

void NumericLiteral::dump(int indent) const
{
    print_indent(indent);
    printf("NumberLiteral %g\n", m_value);
}

void BooleanLiteral::dump(int indent) const
{
    print_indent(indent);
    printf("BooleanLiteral %s\n", m_value ? "true" : "false");
}

void FunctionDeclaration::dump(int indent) const
{
    bool first_time = true;
    StringBuilder parameters_builder;
    for (const auto& parameter : m_parameters) {
        if (first_time)
            first_time = false;
        else
            parameters_builder.append(',');

        parameters_builder.append(parameter);
    }

    print_indent(indent);
    printf("%s '%s(%s)'\n", class_name(), name().characters(), parameters_builder.build().characters());
    body().dump(indent + 1);
}

void ReturnStatement::dump(int indent) const
{
    ASTNode::dump(indent);
    if (argument())
        argument()->dump(indent + 1);
}

void IfStatement::dump(int indent) const
{
    ASTNode::dump(indent);

    print_indent(indent);
    printf("If\n");
    predicate().dump(indent + 1);
    consequent().dump(indent + 1);
    print_indent(indent);
    printf("Else\n");
    alternate().dump(indent + 1);
}

void WhileStatement::dump(int indent) const
{
    ASTNode::dump(indent);

    print_indent(indent);
    printf("While\n");
    predicate().dump(indent + 1);
    body().dump(indent + 1);
}

Value Identifier::execute(Interpreter& interpreter) const
{
    return interpreter.get_variable(string());
}

void Identifier::dump(int indent) const
{
    print_indent(indent);
    printf("Identifier \"%s\"\n", m_string.characters());
}

Value AssignmentExpression::execute(Interpreter& interpreter) const
{
    ASSERT(m_lhs->is_identifier());
    auto name = static_cast<const Identifier&>(*m_lhs).string();
    auto rhs_result = m_rhs->execute(interpreter);

    switch (m_op) {
    case AssignmentOp::Assign:
        interpreter.set_variable(name, rhs_result);
        break;
    }
    return rhs_result;
}

void AssignmentExpression::dump(int indent) const
{
    const char* op_string = nullptr;
    switch (m_op) {
    case AssignmentOp::Assign:
        op_string = "=";
        break;
    }

    ASTNode::dump(indent);
    print_indent(indent + 1);
    printf("%s\n", op_string);
    m_lhs->dump(indent + 1);
    m_rhs->dump(indent + 1);
}

Value VariableDeclaration::execute(Interpreter& interpreter) const
{
    interpreter.declare_variable(name().string(), m_declaration_type);
    if (m_initializer) {
        auto initalizer_result = m_initializer->execute(interpreter);
        interpreter.set_variable(name().string(), initalizer_result);
    }

    return js_undefined();
}

void VariableDeclaration::dump(int indent) const
{
    const char* op_string = nullptr;
    switch (m_declaration_type) {
    case DeclarationType::Let:
        op_string = "Let";
        break;
    case DeclarationType::Var:
        op_string = "Var";
        break;
    }

    ASTNode::dump(indent);
    print_indent(indent + 1);
    printf("%s\n", op_string);
    m_name->dump(indent + 1);
    if (m_initializer)
        m_initializer->dump(indent + 1);
}

void ObjectExpression::dump(int indent) const
{
    ASTNode::dump(indent);
}

void ExpressionStatement::dump(int indent) const
{
    ASTNode::dump(indent);
    m_expression->dump(indent + 1);
}

Value ObjectExpression::execute(Interpreter& interpreter) const
{
    return Value(interpreter.heap().allocate<Object>());
}

void MemberExpression::dump(int indent) const
{
    ASTNode::dump(indent);
    m_object->dump(indent + 1);
    m_property->dump(indent + 1);
}

Value MemberExpression::execute(Interpreter& interpreter) const
{
    auto object_result = m_object->execute(interpreter).to_object(interpreter.heap());
    ASSERT(object_result.is_object());

    String property_name;
    if (m_property->is_identifier()) {
        property_name = static_cast<const Identifier&>(*m_property).string();
    } else {
        ASSERT_NOT_REACHED();
    }

    return object_result.as_object()->get(property_name);
}

Value StringLiteral::execute(Interpreter& interpreter) const
{
    return Value(js_string(interpreter.heap(), m_value));
}

Value NumericLiteral::execute(Interpreter&) const
{
    return Value(m_value);
}

Value BooleanLiteral::execute(Interpreter&) const
{
    return Value(m_value);
}

}