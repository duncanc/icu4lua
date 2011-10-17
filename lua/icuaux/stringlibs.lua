
local select, unpack, type, print
    = select, unpack, type, print
local cowrap, yield
    = coroutine.wrap, coroutine.yield
local U = require 'icu.ustring'
local isustring, Uencode, Udecode
    = U.isustring, U.encode, U.decode
local Ucodepoint, Uchar, Ufind, Uformat, Ugmatch, Ugsub, Ulen, Ulower, Umatch, Urep, Ureverse, Usub, Uupper
    = U.codepoint, U.char, U.find, U.format, U.gmatch, U.gsub, U.len, U.lower, U.match, U.rep, U.reverse, U.sub, U.upper
local strrep
    = string.rep


module(...)


local function listencode(codepage, ...)
    local param_count = select('#', ...)
    local new_params
    for i = 1, param_count do
        local v = select(i, ...)
        if isustring(v) then
            if (param_count == 1) then
                return Uencode(v, codepage)
            end
            new_params = new_params or {...}
            new_params[i] = Uencode(v,codepage)
        end
    end
    if new_params then
        return unpack(new_params,1,param_count)
    end
    return ...
end

local function listdecode(codepage, ...)
    local param_count = select('#', ...)
    local new_params
    for i = 1, param_count do
        local v = select(i, ...)
        if type(v) == 'string' then
            if (param_count == 1) then
                return Udecode(v, codepage)
            end
            new_params = new_params or {...}
            new_params[i] = Udecode(v,codepage)
        end
    end
    if new_params then
        return unpack(new_params,1,param_count)
    end
    return ...
end

function create(codepage)
    return {
        codepoint = function(str,i,j)
            return Ucodepoint(Udecode(str,codepage),i,j)
        end;
        char = function(...)
            return Uencode(Uchar(...),codepage)
        end;
        find = function(s, pattern, init, plain)
            return listencode(codepage, Ufind(Udecode(s,codepage), Udecode(pattern,codepage),init,plain))
        end;
        format = function(formatstring, ...)
            return Uencode(Uformat(U(formatstring,codepage), listdecode(codepage, ...)))
        end;
        gmatch = function(s, pattern)
            local aux = Ugmatch(Udecode(s,codepage), Udecode(pattern,codepage))
            return cowrap(function()
                while true do
                    yield(listencode(codepage, aux()))
                end
            end)
        end;
        gsub = function(s, pattern, repl, n)
            local replt = type(repl)
            if (replt == 'function') then
                local _repl = repl
                repl = function(...)  return listdecode(codepage, _repl(listencode(codepage, ...)));  end
            elseif (replt == 'table') then
                local _repl = repl
                repl = function(...)  return listdecode(codepage, _repl[(listencode(codepage, ...))]);  end
            elseif (replt == 'string') then
                repl = Udecode(repl,codepage)
            end
            return listencode(codepage,Ugsub(Udecode(s,codepage),Udecode(pattern,codepage),repl,n))
        end;
        len = function(s)
            return Ulen(Udecode(s,codepage))
        end;
        lower = function(s)
            return Uencode(Ulower(Udecode(s,codepage)),codepage)
        end;
        match = function(s, pattern, init)
            return listencode(codepage, Umatch(Udecode(s,codepage), Udecode(pattern,codepage), init))
        end;
        rep = strrep;
        reverse = function(s)
            return Uencode(Ureverse(Udecode(s,codepage)),codepage)
        end;
        sub = function(s,i,j)
            return Uencode(Usub(Udecode(s,codepage),i,j),codepage)
        end;
        upper = function(s)
            return Uencode(Uupper(Udecode(s,codepage)),codepage)
        end;
    }
end
