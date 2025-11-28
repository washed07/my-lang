#include <gtest/gtest.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>

class LLVMTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(LLVMTest, LLVMModuleCreation) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create a simple function: int add(int a, int b) { return a + b; }
  llvm::FunctionType *funcType = llvm::FunctionType::get(
      builder.getInt32Ty(), {builder.getInt32Ty(), builder.getInt32Ty()},
      false);
  llvm::Function *addFunction = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, "add", module.get());

  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", addFunction);
  builder.SetInsertPoint(entry);

  auto args = addFunction->args().begin();
  llvm::Value *a = &*args++;
  llvm::Value *b = &*args;

  llvm::Value *sum = builder.CreateAdd(a, b, "sum");
  builder.CreateRet(sum);

  // Verify the function was created correctly
  EXPECT_EQ(addFunction->getName(), "add");
  EXPECT_EQ(addFunction->arg_size(), 2);
}

TEST_F(LLVMTest, LLVMStringRefTest) {
  llvm::StringRef strRef("Hello, LLVM StringRef!");

  // Test size
  EXPECT_EQ(strRef.size(), 22);

  // Test data
  EXPECT_EQ(strRef.data()[0], 'H');
  EXPECT_EQ(strRef.data()[7], 'L');

  // Test substring
  llvm::StringRef subStr = strRef.substr(7, 4);
  EXPECT_EQ(subStr, "LLVM");
}

TEST_F(LLVMTest, LLVMGlobalVariableTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);

  // Create a global variable: int gVar = 100;
  llvm::Type *intType = llvm::Type::getInt32Ty(context);
  llvm::Constant *initValue = llvm::ConstantInt::get(intType, 100);
  llvm::GlobalVariable *gVar = new llvm::GlobalVariable(
      *module, intType, false, llvm::GlobalValue::ExternalLinkage, initValue,
      "gVar");

  // Verify the global variable properties
  EXPECT_EQ(gVar->getName(), "gVar");
  EXPECT_EQ(gVar->getInitializer()->getUniqueInteger().getZExtValue(), 100);
}

TEST_F(LLVMTest, LLVMAPIntTest) {
  // Create an APInt of 32 bits with value 42
  llvm::APInt apInt(32, 42);

  // Test bit width
  EXPECT_EQ(apInt.getBitWidth(), 32);

  // Test value
  EXPECT_EQ(apInt.getZExtValue(), 42);

  // Test addition
  llvm::APInt apInt2(32, 58);
  llvm::APInt sum = apInt + apInt2;
  EXPECT_EQ(sum.getZExtValue(), 100);
}

TEST_F(LLVMTest, LLVMTripleTest) {
  llvm::Triple triple("x86_64-pc-linux-gnu");

  // Test architecture
  EXPECT_EQ(triple.getArch(), llvm::Triple::x86_64);

  // Test vendor
  EXPECT_EQ(triple.getVendor(), llvm::Triple::PC);

  // Test OS
  EXPECT_EQ(triple.getOS(), llvm::Triple::Linux);

  // Test environment
  EXPECT_EQ(triple.getEnvironment(), llvm::Triple::GNU);
}

TEST_F(LLVMTest, LLVMConditionalBranchTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create a function: int max(int a, int b) { return a > b ? a : b; }
  llvm::FunctionType *funcType = llvm::FunctionType::get(
      builder.getInt32Ty(), {builder.getInt32Ty(), builder.getInt32Ty()},
      false);
  llvm::Function *maxFunction = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, "max", module.get());

  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", maxFunction);
  llvm::BasicBlock *thenBB =
      llvm::BasicBlock::Create(context, "then", maxFunction);
  llvm::BasicBlock *elseBB =
      llvm::BasicBlock::Create(context, "else", maxFunction);
  llvm::BasicBlock *mergeBB =
      llvm::BasicBlock::Create(context, "merge", maxFunction);

  builder.SetInsertPoint(entry);
  auto args = maxFunction->args().begin();
  llvm::Value *a = &*args++;
  llvm::Value *b = &*args;

  llvm::Value *cond = builder.CreateICmpSGT(a, b, "cmp");
  builder.CreateCondBr(cond, thenBB, elseBB);

  builder.SetInsertPoint(thenBB);
  builder.CreateBr(mergeBB);

  builder.SetInsertPoint(elseBB);
  builder.CreateBr(mergeBB);

  builder.SetInsertPoint(mergeBB);
  llvm::PHINode *phi = builder.CreatePHI(builder.getInt32Ty(), 2, "result");
  phi->addIncoming(a, thenBB);
  phi->addIncoming(b, elseBB);
  builder.CreateRet(phi);

  // Verify the function structure
  EXPECT_EQ(maxFunction->size(), 4);
  EXPECT_TRUE(phi->getNumIncomingValues() == 2);
}

TEST_F(LLVMTest, LLVMFunctionCallTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create a helper function: int double(int x) { return x * 2; }
  llvm::FunctionType *doubleFuncType = llvm::FunctionType::get(
      builder.getInt32Ty(), {builder.getInt32Ty()}, false);
  llvm::Function *doubleFunc = llvm::Function::Create(
      doubleFuncType, llvm::Function::ExternalLinkage, "double", module.get());

  llvm::BasicBlock *doubleEntry =
      llvm::BasicBlock::Create(context, "entry", doubleFunc);
  builder.SetInsertPoint(doubleEntry);
  llvm::Value *x = &*doubleFunc->args().begin();
  llvm::Value *two = builder.getInt32(2);
  llvm::Value *result = builder.CreateMul(x, two, "result");
  builder.CreateRet(result);

  // Create a caller function: int quadruple(int x) { return double(double(x));
  // }
  llvm::FunctionType *quadFuncType = llvm::FunctionType::get(
      builder.getInt32Ty(), {builder.getInt32Ty()}, false);
  llvm::Function *quadFunc = llvm::Function::Create(
      quadFuncType, llvm::Function::ExternalLinkage, "quadruple", module.get());

  llvm::BasicBlock *quadEntry =
      llvm::BasicBlock::Create(context, "entry", quadFunc);
  builder.SetInsertPoint(quadEntry);
  llvm::Value *arg = &*quadFunc->args().begin();
  llvm::Value *call1 = builder.CreateCall(doubleFunc, {arg}, "call1");
  llvm::Value *call2 = builder.CreateCall(doubleFunc, {call1}, "call2");
  builder.CreateRet(call2);

  // Verify function calls
  EXPECT_EQ(module->getFunctionList().size(), 2);
  EXPECT_EQ(quadFunc->getName(), "quadruple");
}

TEST_F(LLVMTest, LLVMArrayTypeTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create an array type: [10 x i32]
  llvm::Type *arrayType = llvm::ArrayType::get(builder.getInt32Ty(), 10);

  // Create a global array variable
  llvm::Constant *arrayInit = llvm::ConstantAggregateZero::get(arrayType);
  llvm::GlobalVariable *globalArray = new llvm::GlobalVariable(
      *module, arrayType, false, llvm::GlobalValue::InternalLinkage, arrayInit,
      "myArray");

  // Verify array properties
  EXPECT_TRUE(arrayType->isArrayTy());
  EXPECT_EQ(llvm::cast<llvm::ArrayType>(arrayType)->getNumElements(), 10);
  EXPECT_EQ(globalArray->getName(), "myArray");
}

TEST_F(LLVMTest, LLVMStructTypeTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create a struct type: struct Point { int x; int y; }
  llvm::Type *intTy = builder.getInt32Ty();
  llvm::StructType *pointType =
      llvm::StructType::create(context, {intTy, intTy}, "Point");

  // Verify struct properties
  EXPECT_TRUE(pointType->isStructTy());
  EXPECT_EQ(pointType->getNumElements(), 2);
  EXPECT_EQ(pointType->getName(), "Point");
}

TEST_F(LLVMTest, LLVMPointerTypeTest) {
  llvm::LLVMContext context;
  llvm::IRBuilder<> builder(context);

  // Create pointer type
  llvm::Type *intPtrType = builder.getPtrTy();

  // Verify pointer properties
  EXPECT_TRUE(intPtrType->isPointerTy());
}

TEST_F(LLVMTest, LLVMConstantExprTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create constant integers
  llvm::Constant *c1 = builder.getInt32(10);
  llvm::Constant *c2 = builder.getInt32(20);

  // Create constant expression: 10 + 20
  llvm::Constant *sum = llvm::ConstantExpr::getAdd(c1, c2);

  // Verify constant values
  EXPECT_EQ(llvm::cast<llvm::ConstantInt>(c1)->getZExtValue(), 10);
  EXPECT_EQ(llvm::cast<llvm::ConstantInt>(c2)->getZExtValue(), 20);
  EXPECT_EQ(llvm::cast<llvm::ConstantInt>(sum)->getZExtValue(), 30);
}

TEST_F(LLVMTest, LLVMFloatTypeTest) {
  llvm::LLVMContext context;
  llvm::IRBuilder<> builder(context);

  // Create float constant
  llvm::Constant *floatVal =
      llvm::ConstantFP::get(context, llvm::APFloat(3.14f));

  // Verify float properties
  EXPECT_TRUE(floatVal->getType()->isFloatTy());
  EXPECT_NEAR(
      llvm::cast<llvm::ConstantFP>(floatVal)->getValueAPF().convertToFloat(),
      3.14f, 0.001f);
}

TEST_F(LLVMTest, LLVMVectorTypeTest) {
  llvm::LLVMContext context;
  llvm::IRBuilder<> builder(context);

  // Create a vector type: <4 x i32>
  llvm::Type *vecType = llvm::FixedVectorType::get(builder.getInt32Ty(), 4);

  // Verify vector properties
  EXPECT_TRUE(vecType->isVectorTy());
  EXPECT_EQ(llvm::cast<llvm::FixedVectorType>(vecType)->getNumElements(), 4);
}

TEST_F(LLVMTest, LLVMLoopTest) {
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module =
      std::make_unique<llvm::Module>("test_module", context);
  llvm::IRBuilder<> builder(context);

  // Create a function with a simple loop
  llvm::FunctionType *funcType = llvm::FunctionType::get(
      builder.getInt32Ty(), {builder.getInt32Ty()}, false);
  llvm::Function *loopFunc = llvm::Function::Create(
      funcType, llvm::Function::ExternalLinkage, "sum_to_n", module.get());

  llvm::BasicBlock *entry =
      llvm::BasicBlock::Create(context, "entry", loopFunc);
  llvm::BasicBlock *loopHeader =
      llvm::BasicBlock::Create(context, "loop.header", loopFunc);
  llvm::BasicBlock *loopBody =
      llvm::BasicBlock::Create(context, "loop.body", loopFunc);
  llvm::BasicBlock *loopExit =
      llvm::BasicBlock::Create(context, "loop.exit", loopFunc);

  builder.SetInsertPoint(entry);
  llvm::Value *n = &*loopFunc->args().begin();
  builder.CreateBr(loopHeader);

  builder.SetInsertPoint(loopHeader);
  llvm::PHINode *i = builder.CreatePHI(builder.getInt32Ty(), 2, "i");
  llvm::PHINode *sum = builder.CreatePHI(builder.getInt32Ty(), 2, "sum");
  i->addIncoming(builder.getInt32(0), entry);
  sum->addIncoming(builder.getInt32(0), entry);

  llvm::Value *cond = builder.CreateICmpSLT(i, n, "cond");
  builder.CreateCondBr(cond, loopBody, loopExit);

  builder.SetInsertPoint(loopBody);
  llvm::Value *newSum = builder.CreateAdd(sum, i, "new.sum");
  llvm::Value *nextI = builder.CreateAdd(i, builder.getInt32(1), "next.i");
  i->addIncoming(nextI, loopBody);
  sum->addIncoming(newSum, loopBody);
  builder.CreateBr(loopHeader);

  builder.SetInsertPoint(loopExit);
  builder.CreateRet(sum);

  // Verify loop structure
  EXPECT_EQ(loopFunc->size(), 4);
  EXPECT_EQ(i->getNumIncomingValues(), 2);
  EXPECT_EQ(sum->getNumIncomingValues(), 2);
}
