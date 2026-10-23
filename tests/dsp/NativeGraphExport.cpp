#include "../../src/bridge/AstJson.h"
#include "../../src/third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char**argv) {
 if(argc!=2)return 2;
 std::ifstream input(argv[1]);nlohmann::json catalog;input>>catalog;
 nlohmann::json result=nlohmann::json::array();
 for(const auto& p:catalog) {
  dsl::GraphDocument doc,copy;juce::String error;
  const auto script=p.at("script").get<std::string>();
  if(!dsl::parse(juce::String(script),doc,error)) {std::cerr<<p.at("name")<<": "<<error<<"\n";return 1;}
  const auto ast=dsl::toJson(doc);
  if(!dsl::fromJson(ast,copy,error)||!dsl::semanticallyEqual(doc,copy))return 1;
  result.push_back({{"name",p.at("name")},{"ast",nlohmann::json::parse(ast.toStdString())}});
 }
 std::cout<<result.dump();
}
