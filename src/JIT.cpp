#include "JIT.hpp"
#include "CodeGenerator.hpp"

using namespace DecafJIT;

llvm::ExitOnError JIT::exitOnError;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> JIT::JIT_;

void JIT::initJIT() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  JIT::JIT_ = exitOnError(llvm::orc::KaleidoscopeJIT::Create());
}

void DecafJIT::handleFuncDefinition(DecafParsing::Parser* parser) {
  if (auto fnAST = parser->parseFuncDefinition()) {
    DecafLogger::Logger::displayASTExpr(*fnAST->body);
    if (auto *fnIR = fnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      fnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      JIT::exitOnError(JIT::JIT_->addModule(
          llvm::orc::ThreadSafeModule(std::move(DecafCodeGen::CodeGenerator::module_), std::move(DecafCodeGen::CodeGenerator::context))));
      DecafCodeGen::CodeGenerator::initializeModuleAndPassManager();
    }
  } else {
    // Skip token for error recovery.
    parser->consume();
  }
}

double DecafJIT::handleTopLevelStatement(DecafParsing::Parser* parser) {
  DecafLogger::Logger::displayToken(parser->peek().value());
  if (auto fnAST = parser->parseTopLevelExpr()) {
    DecafLogger::Logger::displayASTExpr(*fnAST->body);
    if (fnAST->codegen()) {
      auto RT = DecafJIT::JIT::JIT_->getMainJITDylib().createResourceTracker();
      auto TSM = llvm::orc::ThreadSafeModule(std::move(DecafCodeGen::CodeGenerator::module_), std::move(DecafCodeGen::CodeGenerator::context));
      DecafJIT::JIT::exitOnError(DecafJIT::JIT::JIT_->addModule(std::move(TSM), RT));
      DecafCodeGen::CodeGenerator::initializeModuleAndPassManager();
      auto exprSymbol = DecafJIT::JIT::exitOnError(DecafJIT::JIT::JIT_->lookup("__anon_expr"));

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = exprSymbol.getAddress().toPtr<double (*)()>();
      // double (*FP)() = (double (*)())(intptr_t)exprSymbol.getAddress();
      double result = FP();
      // fprintf(stderr, "Evaluated to \e[1;37;41m%f\e[m\n\n", FP());
      // Delete the anonymous expression module from the JIT.
      DecafJIT::JIT::exitOnError(RT->remove());
      return result;
    }
  } else {
    // Skip token for error recovery
    parser->consume();
    return -1.0;
  }
}
