#include "CodeGenerator.hpp"
#include "Lexer.hpp"
#include "Logger.hpp"
#include "JIT.hpp"

#define DEBUG_LINE std::cout << __LINE__ << " ran." << std::endl;

using namespace DecafCodeGen;
using namespace DecafScanning;

// Open a new context and module.
std::unique_ptr<llvm::LLVMContext> CodeGenerator::context;
std::unique_ptr<llvm::Module> CodeGenerator::module_;

// Create a new builder for the module.
std::unique_ptr<llvm::IRBuilder<>> CodeGenerator::builder;
std::map<std::string, llvm::Value*> CodeGenerator::namedValues;
std::map<std::string, std::unique_ptr<DecafParsing::AST::Prototype>> CodeGenerator::functionProtos;

std::unique_ptr<llvm::FunctionPassManager> CodeGenerator::FPM = std::make_unique<llvm::FunctionPassManager>();
std::unique_ptr<llvm::LoopAnalysisManager> CodeGenerator::LAM = std::make_unique<llvm::LoopAnalysisManager>();
std::unique_ptr<llvm::FunctionAnalysisManager> CodeGenerator::FAM = std::make_unique<llvm::FunctionAnalysisManager>();
std::unique_ptr<llvm::CGSCCAnalysisManager> CodeGenerator::CGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
std::unique_ptr<llvm::ModuleAnalysisManager> CodeGenerator::MAM = std::make_unique<llvm::ModuleAnalysisManager>();
std::unique_ptr<llvm::PassInstrumentationCallbacks> CodeGenerator::PIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
std::unique_ptr<llvm::StandardInstrumentations> CodeGenerator::SI;
llvm::PassBuilder CodeGenerator::PB;

llvm::Function *getFunction(std::string name) {
  // First, see if the function has already been added to the current module.
  if (auto *F = CodeGenerator::module_->getFunction(name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = CodeGenerator::functionProtos.find(name);
  if (FI != CodeGenerator::functionProtos.end())
    return FI->second->codegen();

  // If no existing prototype exists, return null.
  return nullptr;
}

void CodeGenerator::initializeModuleAndPassManager() {
  // Open a new context and module.
  CodeGenerator::context = std::make_unique<llvm::LLVMContext>();
  CodeGenerator::module_ = std::make_unique<llvm::Module>("LASIL_JIT", *CodeGenerator::context);
  CodeGenerator::module_->setDataLayout(DecafJIT::JIT::JIT_->getDataLayout());

  // Create a new builder for the module.
  CodeGenerator::builder = std::make_unique<llvm::IRBuilder<>>(*CodeGenerator::context);

  CodeGenerator::SI = std::make_unique<llvm::StandardInstrumentations>(*CodeGenerator::context, /*DebugLogging*/ true);
  CodeGenerator::SI->registerCallbacks(*CodeGenerator::PIC, CodeGenerator::MAM.get());

  // Add transform passes.
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  CodeGenerator::FPM->addPass(llvm::InstCombinePass());
  // Reassociate expressions.
  CodeGenerator::FPM->addPass(llvm::ReassociatePass());
  // Eliminate Common SubExpressions.
  CodeGenerator::FPM->addPass(llvm::GVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  CodeGenerator::FPM->addPass(llvm::SimplifyCFGPass());

  // Register analysis passes used in these transform passes.
  CodeGenerator::PB.registerModuleAnalyses(*CodeGenerator::MAM);
  CodeGenerator::PB.registerFunctionAnalyses(*CodeGenerator::FAM);
  CodeGenerator::PB.crossRegisterProxies(*CodeGenerator::LAM, *CodeGenerator::FAM, *CodeGenerator::CGAM, *CodeGenerator::MAM);
}

namespace DecafParsing {

namespace AST {

llvm::Value *NumberExpr::codegen() {
  return llvm::ConstantFP::get(*CodeGenerator::context, llvm::APFloat(value));
}

llvm::Value *VariableExpr::codegen() {
  // Look this variable up in the function.
  llvm::Value *V = CodeGenerator::namedValues[name];
  if (!V)
    std::cout << "Unknown variable name" << std::endl; // To-do: Log error
  return V;
}

llvm::Value *BinaryExpr::codegen() {
  llvm::Value *L = LHS->codegen();
  llvm::Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  switch (op.type) {
    case TokenType::PLUS:
      return CodeGenerator::builder->CreateFAdd(L, R, "addtmp");
    case TokenType::MINUS:
      return CodeGenerator::builder->CreateFSub(L, R, "subtmp");
    case TokenType::TIMES:
      return CodeGenerator::builder->CreateFMul(L, R, "multmp");
    case TokenType::LESS_THAN:
      L = CodeGenerator::builder->CreateFCmpULT(L, R, "cmptmp");
      // Convert bool 0/1 to double 0.0 or 1.0
      return CodeGenerator::builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*CodeGenerator::context),
                                  "booltmp");
    // default:
    //   return LogErrorV("invalid binary operator");
  }

  return nullptr; // To-do fix switch default
}

llvm::Value *CallExpr::codegen() {
  // Look up the name in the global module table.
  llvm::Function *calleeF = getFunction(callee);
  if (!calleeF) std::cout << "Unknown function referenced" << std::endl; // To-do: Throw error
  //   return LogErrorV("Unknown function referenced");

  // // If argument mismatch error.
  // if (calleeF->arg_size() != Args.size())
  //   return LogErrorV("Incorrect # arguments passed");

  std::vector<llvm::Value *> argsV;
  for (unsigned i = 0, e = args.size(); i != e; ++i) {
    argsV.push_back(args[i]->codegen());
    if (!argsV.back())
      return nullptr;
  }

  return CodeGenerator::builder->CreateCall(calleeF, argsV, "calltmp");
}

llvm::Value *IfExpr::codegen() {
  llvm::Value *condV = cond->codegen();
  if (!condV)
    return nullptr;

  // Convert condition to a bool by comparing non-equal to 0.0.
  condV = CodeGenerator::builder->CreateFCmpONE(
      condV, llvm::ConstantFP::get(*CodeGenerator::context, llvm::APFloat(0.0)), "ifcond");

  llvm::Function *function = CodeGenerator::builder->GetInsertBlock()->getParent();

  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(*CodeGenerator::context, "then", function);
  llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(*CodeGenerator::context, "else");
  llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(*CodeGenerator::context, "ifcont");

  CodeGenerator::builder->CreateCondBr(condV, thenBB, elseBB);

  // Emit then value
  CodeGenerator::builder->SetInsertPoint(thenBB);

  llvm::Value *thenV = then->codegen();
  if (!thenV)
    return nullptr;

  CodeGenerator::builder->CreateBr(mergeBB);
  // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
  thenBB = CodeGenerator::builder->GetInsertBlock();

  // Emit else block.
  function->insert(function->end(), elseBB);
  CodeGenerator::builder->SetInsertPoint(elseBB);

  llvm::Value *elseV = else_->codegen();
  if (!elseV)
    return nullptr;

  CodeGenerator::builder->CreateBr(mergeBB);
  // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
  elseBB = CodeGenerator::builder->GetInsertBlock();

  // Emit merge block.
  function->insert(function->end(), mergeBB);
  CodeGenerator::builder->SetInsertPoint(mergeBB);
  llvm::PHINode *PN = CodeGenerator::builder->CreatePHI(llvm::Type::getDoubleTy(*CodeGenerator::context), 2, "iftmp");

  PN->addIncoming(thenV, thenBB);
  PN->addIncoming(elseV, elseBB);
  return PN;
}

llvm::Value *WhileExpr::codegen() {
  llvm::Function *function = CodeGenerator::builder->GetInsertBlock()->getParent();
  std::cout << "bruh " << __LINE__ << std::endl;

  llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*CodeGenerator::context, "entrywhile", function);
  llvm::BasicBlock *loopBB = llvm::BasicBlock::Create(*CodeGenerator::context, "loopwhile");
  llvm::BasicBlock *endBB = llvm::BasicBlock::Create(*CodeGenerator::context, "endwhile");
  std::cout << "bruh " << __LINE__ << std::endl;

  // Insert an explicit fall through from the current block to the entry
  CodeGenerator::builder->CreateBr(loopBB);
  std::cout << "bruh " << __LINE__ << std::endl;

  // Loop entry
  std::cout << "bruh " << __LINE__ << std::endl;
  CodeGenerator::builder->SetInsertPoint(loopBB);
  llvm::Value *condV = cond->codegen();
  if (!condV)
    return nullptr; // To-do: throw error
  condV = CodeGenerator::builder->CreateFCmpONE(
    condV, llvm::ConstantFP::get(*CodeGenerator::context, llvm::APFloat(0.0)), "loopcmp");
  CodeGenerator::builder->CreateCondBr(condV, loopBB, endBB);
  entryBB = CodeGenerator::builder->GetInsertBlock();
  std::cout << "bruh " << __LINE__ << std::endl;

  // Loop body
  function->insert(function->end(), loopBB);
  CodeGenerator::builder->SetInsertPoint(loopBB);
  llvm::Value* bodyVal = body->codegen();
  std::cout << "bruh " << __LINE__ << std::endl;
  if (!bodyVal) 
    return nullptr;
  CodeGenerator::builder->CreateBr(entryBB);
  std::cout << "bruh " << __LINE__ << std::endl;

  // Loop end
  // function->getBasicBlockList().push_back(endBB);
  function->insert(function->end(),endBB);
  CodeGenerator::builder->SetInsertPoint(endBB);
  std::cout << "bruh " << __LINE__ << std::endl;

  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*CodeGenerator::context));
}

llvm::Function *Prototype::codegen() {
  std::vector<llvm::Type*> doubles(args.size(),
                             llvm::Type::getDoubleTy(*CodeGenerator::context));
  llvm::FunctionType *FT =
    llvm::FunctionType::get(llvm::Type::getDoubleTy(*CodeGenerator::context), doubles, false);

  llvm::Function *F =
    llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, CodeGenerator::module_.get());

  // Set names for all arguments.
  unsigned i = 0;
  for (auto &arg : F->args())
    arg.setName(args[i++]);

  return F;
}

llvm::Function *Function::codegen() {
  // First, check for an existing function from a previous 'extern' declaration
  auto &P = *proto;
  CodeGenerator::functionProtos[proto->getName()] = std::move(proto);
  llvm::Function *theFunction = getFunction(P.getName());

  if (!theFunction) // To-do: Throw error
    return nullptr;
  
  // Create a new basic block to start insertion into
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*CodeGenerator::context, "entry", theFunction);
  CodeGenerator::builder->SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map
  CodeGenerator::namedValues.clear();
  for (auto &arg : theFunction->args())
    CodeGenerator::namedValues[std::string(arg.getName())] = &arg;

  if (llvm::Value *retVal = body->codegen()) {
    // Finish off the function
    CodeGenerator::builder->CreateRet(retVal);

    // Validate the generated code, checking for consistency
    llvm::verifyFunction(*theFunction);

    DecafLogger::Logger::logMessage(DecafLogger::LogType::DEBUG_INFO, "Unoptimized function");
    theFunction->print(llvm::errs());
    fprintf(stderr, "\n");

    DecafLogger::Logger::logMessage(DecafLogger::LogType::DEBUG_INFO, "Optimized function");
    CodeGenerator::FPM->run(*theFunction, *CodeGenerator::FAM);
    theFunction->print(llvm::errs());
    fprintf(stderr, "\n");

    return theFunction;
  }

  // Error reading body, remove function
  theFunction->eraseFromParent();
  return nullptr; // To-do: Throw error
}

}

}
