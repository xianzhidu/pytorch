#include <torch/nn/module.h>

#include <torch/nn/cursor.h>

#include <torch/csrc/autograd/generated/VariableType.h>

#include <ATen/Error.h>

#include <algorithm>
#include <map>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace torch {
namespace nn {

Module::Module(std::string name) : name_(std::move(name)) {}

const std::string& Module::name() const noexcept {
  // If the name optional is empty at this point, we grab the name of the
  // dynamic type via RTTI. Note that we cannot do this in the constructor,
  // because in the constructor of a base class `this` always refers to the base
  // type. Inheritance effectively does not work in constructors. Also this note
  // from http://en.cppreference.com/w/cpp/language/typeid:
  // If typeid is used on an object under construction or destruction (in a
  // destructor or in a constructor, including constructor's initializer list
  // or default member initializers), then the std::type_info object referred
  // to by this typeid represents the class that is being constructed or
  // destroyed even if it is not the most-derived class.
  if (!name_.has_value()) {
    name_ = at::demangle(typeid(*this).name());
  }
  return *name_;
}

std::shared_ptr<Module> Module::clone() const {
  AT_ERROR(
      "clone() has not been implemented for ",
      name(),
      ". Use the copy constructor if you don't require polymorphic cloning. "
      "Otherwise, subclass torch::nn::CloneableModule<",
      name(),
      "> instead of torch::nn::Module to inherit the ability to clone.");
}

ModuleCursor Module::modules() {
  return ModuleCursor(*this);
}

ConstModuleCursor Module::modules() const {
  return ConstModuleCursor(*this);
}

ModuleCursor Module::children() {
  return ModuleCursor(*this, /*maximum_depth=*/1);
}

ConstModuleCursor Module::children() const {
  return ConstModuleCursor(*this, /*maximum_depth=*/1);
}

ParameterCursor Module::parameters() {
  return ParameterCursor(*this);
}

ConstParameterCursor Module::parameters() const {
  return ConstParameterCursor(*this);
}

BufferCursor Module::buffers() {
  return BufferCursor(*this);
}

ConstBufferCursor Module::buffers() const {
  return ConstBufferCursor(*this);
}

void Module::train() {
  for (auto& child : children_) {
    child.value->train();
  }
  is_training_ = true;
}

void Module::eval() {
  for (auto& child : children_) {
    child.value->eval();
  }
  is_training_ = false;
}

void Module::cuda() {
  to(at::kCUDA);
}

void Module::cpu() {
  to(at::kCPU);
}

void Module::to(at::Type& type) {
  for (auto& child : children_) {
    child.value->to(type);
  }
  for (auto& parameter : parameters_) {
    at::detail::set_data(*parameter, parameter->data().toType(type));
    AT_ASSERT(parameter->data().type() == type);
    AT_ASSERT(&parameter->type() == autograd::VariableType::getType(type));
  }
}

void Module::to(at::ScalarType scalar_type) {
  for (auto& child : children_) {
    child.value->to(scalar_type);
  }
  for (auto& parameter : parameters_) {
    auto& new_type = parameter->data().type().toScalarType(scalar_type);
    at::detail::set_data(*parameter, parameter->data().toType(new_type));
    AT_ASSERT(parameter->data().type().scalarType() == scalar_type);
    AT_ASSERT(parameter->type().scalarType() == scalar_type);
  }
}

void Module::to(at::Backend backend) {
  for (auto& child : children_) {
    child.value->to(backend);
  }
  for (auto& parameter : parameters_) {
    auto& new_type = parameter->data().type().toBackend(backend);
    at::detail::set_data(*parameter, parameter->data().toType(new_type));
    AT_ASSERT(parameter->data().type().backend() == backend);
    AT_ASSERT(parameter->type().backend() == backend);
  }
}

bool Module::is_training() const noexcept {
  return is_training_;
}

void Module::zero_grad() {
  for (auto& child : children_) {
    child.value->zero_grad();
  }
  for (auto& parameter : parameters_) {
    parameter->grad().zero_();
  }
}

autograd::Variable& Module::register_parameter(
    std::string name,
    Variable tensor,
    bool requires_grad) {
  tensor.set_requires_grad(requires_grad);
  return parameters_.insert(std::move(name), std::move(tensor));
}

autograd::Variable& Module::register_buffer(std::string name, Variable tensor) {
  return parameters_.insert(std::move(name), std::move(tensor));
}

void Module::clone_(Module& other) {}
} // namespace nn
} // namespace torch
