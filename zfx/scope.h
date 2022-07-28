//
// Created by admin on 2022/7/26.
//
#include <map>
#include <memory>
#include <string>

class Scope {
    //作用域因为目前zfx还没搞 {}和支持函数,所以作用域目前来说只有一个全局作用域
    static uint32_t id;
    std::map<std::string> name2sym; // 比如 @clr key是clr, value就是一个Symbol  $T key是 T, value 是Param

    Scope() = default;
    //上级作用域
    std::shared_ptr<Scope> enclosingScope;

    /*
     * 把符号加入到符号表，在遍历ast的时候
     *
     * */

    void enter() {

    }

    /*
     * 查询是否有某名称的符号
     * @param name
     * */
    bool hasSymbol() {

    }

    /*
     * 根据名称查找是否包含， 如果包含则返回，没有返回nullptr
     * */
    std::shared_ptr<> getSymbol(const std::string& name) {
        auto sym = name2sym.find(name);
        if (sym != this->name2sym.end()) {
            return sym->second;
        } else {
            return nullptr;
        }

        //其实可以返回一个optional后面改一下
    }

    //级连查找某一个@ $,当前作用域里没有就依次往上级查找， 但是目前还没搞{}
    std::shared_ptr<> getSymbolCache() {

    }
};

uint32_t Scope::id = 0;