#include "reflection/reflection.h"

#include <map>
#include "gtest.h"
#include "util/spinlock.h"
#include "util/singleton.h"
#include "skse64/PapyrusVM.h"

#include "reflection/detail/code_producer.hpp"
#include "reflection/detail/type_traits.hpp"

namespace reflection {

    static auto makeDB = []() {
        std::map<istring, class_info> classDB;

        for (auto & item : meta<class_info_creator>::getListConst()) {
            class_info& info = item();

            auto found = classDB.find(info.className());
            if (found != classDB.end()) {
                found->second.merge_with_extension(info);
            }
            else {
                classDB[info.className()] = info;
            }
        }

        return classDB;
    };


    static util::singleton<std::map<istring, class_info> > g_class_registry_map(
        []() {
            return new std::map<istring, class_info>{ makeDB() };
        }
    );

    const std::map<istring, class_info>& class_registry() {
        return g_class_registry_map.get();
    }

    const function_info* find_function_of_class(const char * functionName, const char *className) {

        const function_info * fInfo = nullptr;

        auto& db = class_registry();
        auto itr = db.find(className);
        if (itr != db.end()) {
            auto& cls = itr->second;
            fInfo = cls.find_function(functionName);
        }

        return fInfo;
    }

    class_info amalgamate_classes(const std::string& amalgName, const std::map<istring, class_info>& classes) {
        class_info amalgam{ amalgName.c_str() };

        for (const auto& cls : classes) {
            for (const auto& func : cls.second.methods) {
                if (!func.isStateless()) {
                    function_info info{ func };
                    info.setComment(nullptr);
                    info.name = cls.second.className() + '_' + func.name;
                    amalgam.addFunction(std::move(info));
                }
            }
        }

        return amalgam;
    }

    TEST(reflection, _)
    {

        class test_class : public reflection::class_meta_mixin_t<test_class> {
        public:

            REGISTER_TES_NAME("test_class");

            test_class() {
                metaInfo.comment = "a class to test reflection";
            }

            static void nothing() {}
            REGISTERF2_STATELESS(nothing, "", "does absolutely nothing");
        };

        TES_META_INFO(test_class);

        // just test whether is doesn't crash and not empty
        auto db = makeDB();

        EXPECT_TRUE(db.find("test_class") != db.end());
        const class_info& cls = db.find("test_class")->second;

        EXPECT_TRUE(cls.className() == "test_class");

        auto func = cls.find_function("nothing");
        EXPECT_TRUE(func != nullptr);
        EXPECT_TRUE(func->name == "nothing");
        EXPECT_TRUE(func->c_func == &test_class::nothing);
    }
}
