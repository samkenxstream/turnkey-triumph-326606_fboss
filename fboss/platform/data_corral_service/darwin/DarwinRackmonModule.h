// Copyright 2014-present Facebook. All Rights Reserved.

#pragma once

#include <fboss/platform/data_corral_service/FruModule.h>

namespace facebook::fboss::platform::data_corral_service {

class DarwinRackmonModule : public FruModule {
 public:
  explicit DarwinRackmonModule(int id) : FruModule(id) {}
  void refresh() override;
  void init() override;
  std::string getName() override;
};

} // namespace facebook::fboss::platform::data_corral_service