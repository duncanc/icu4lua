
local type, getmetatable, setmetatable, pairs, ipairs, tostring, next, error, select, tonumber, print
	= type, getmetatable, setmetatable, pairs, ipairs, tostring, next, error, select, tonumber, print
local gmatch, strformat, strmatch, strfind, strbyte, strsub
	= string.gmatch, string.format, string.match, string.find, string.byte, string.sub
local getmetatable = getmetatable
local pairs = pairs
local U = require 'icu.ustring'
local utf8 = require 'icu.utf8'
local isustring = U.isustring
local stringbuilder = require 'stringbuilder'


module (...)


local function writejsonstring(v, out)
	out '"'
	if isustring(v) then
		
	else
		for c in gmatch(tostring(v), '.') do
			if (c == '\r') then
				out '\\r'
			elseif (c == '\n') then
				out '\\n'
			elseif (c == '\t') then
				out '\\t'
			elseif (c == '\b') then
				out '\\b'
			elseif (c == '\f') then
				out '\\f'
			elseif (c < ' ') then
				out(strformat('\\u%04X', strbyte(c)))
			else
				if (c == '"') or (c == '\\') then
					out '\\'
				end
				out(c)
			end
		end
	end
	out '"'
end

local function writejsonvalue(v, out)
	local m = getmetatable(v)
	if (m and m.__writejsonvalue) then
		m.__writejsonvalue(v, out)
		return
	end
	local t = type(v)
	if (t == 'table') then
		out '{'
		for key,value in pairs(v) do
			writejsonstring(key, out)
			out ':'
			writejsonvalue(value, out)
			if (next(v,key) ~= nil) then  out ',';  end
		end
		out '}'
	elseif (t == 'string') then
		writejsonstring(v, out)
	elseif (t == 'boolean') or (t == 'number') then
		out(tostring(v))
	elseif (t == 'nil') then
		out 'null'
	else
		error('cannot convert to json: ' .. tostring(v),2)
	end
end

local _encode
function encode(v)
	local out = stringbuilder()
	writejsonvalue(v, out)
	return tostring(out)
end
_encode = encode

local array_meta = {
	__writejsonvalue = function(self, out)
		out '['
		for i,v in ipairs(self) do
			if (i > 1) then  out ',';  end
			writejsonvalue(v, out)
		end
		out ']'
	end;
	__tostring = function(self)
		local out = stringbuilder()
		out 'json.array: '
		writejsonvalue(self,out)
		return tostring(out)
	end;
}

function array(...)
	local argcount = select('#', ...)
	local m = getmetatable((...))
	if (argcount == 0) then
		return setmetatable({}, array_meta)
	elseif (argcount > 1) or (type(...) ~= 'table') or (m and m ~= array_meta) then
		error('json.array() expects to take either no parameters or a plain table containing the array values', 2)
	end
	return setmetatable((...), array_meta)
end

function object(...)
	local argcount = select('#', ...)
	local m = getmetatable((...))
	if (argcount == 0) then
		return setmetatable({}, object_meta)
	elseif (argcount > 1) or (type(...) ~= 'table') or (m and m ~= array_meta) then
		error('json.object() expects to take either no parameters or one table containing the object values', 2)
	end
	return setmetatable((...), array_meta)
end

local null_meta = {__writejsonvalue = function(me,out)  out 'null';  end, __tostring = function()  return "json.null";  end}
null_meta.__metatable = null_meta
null = setmetatable({}, null_meta)

function isnull(v)
	return (v == null) or (v == nil)
end

local function jsonstringat(json, pos, elevel)
	local c
	pos = pos+1
	local start = pos
	while true do
		local after_esc = strmatch(json, '^[^"\\]*\\.()', pos)
		if after_esc then
			pos = after_esc
			c = nil
		else
			pos = strmatch(json, '"()', pos)
			if not pos then
				error('unterminated string', elevel)
			end
			break
		end
	end
	local v = utf8.unescape(strsub(json,start,pos-2))
	if not v then
		error('invalid string content', elevel)
	end
	return v, pos
end

local object_meta = {__tostring = function(me)  local out = stringbuilder(); out 'json.object: '; writejsonvalue(me, out); return tostring(out);  end}

local function jsonvalueat(json, pos, elevel)
	local c = json:sub(pos, pos)
	if (c == '{') then
		local obj = {}
		pos, c = strmatch(json, '()(%S)', pos+1)
		if (c == '}') then
			return setmetatable(obj, object_meta), pos+1
		end
		while true do
			if not pos then
				error('unterminated object',elevel)
			end
			local key; key, pos = jsonstringat(json, pos, elevel+1)
			pos = strmatch(json, '^%s*:%s*()', pos)
			if not pos then
				error('unrecognised content in object',elevel)
			end
			obj[key], pos = jsonvalueat(json,pos,elevel+1)
			local symbol; symbol, pos = strmatch(json, '(%S)()', pos)
			if (symbol == '}') then
				break
			elseif (symbol ~= ',') then
				error('unrecognised content in object',elevel)
			end
			pos = strmatch(json, '()%S', pos)
		end
		return setmetatable(obj, object_meta), pos
	elseif (c == '[') then
		local arr = array()
		local i = 1
		pos, c = strmatch(json, '()(%S)', pos+1)
		if (c == ']') then
			return arr, pos+1
		end
		while true do
			if not pos then
				error('unterminated array', elevel)
			end
			arr[i], pos = jsonvalueat(json, pos, elevel+1)
			i = i + 1
			c, pos = strmatch(json, '(%S)()', pos)
			if (c == ']') then
				break
			elseif (c ~= ',') then
				error('unrecognised content in array', elevel)
			end
			pos, c = strmatch(json, '()(%S)', pos)
		end
		return arr, pos
	elseif (c == '"') then
		return jsonstringat(json, pos, elevel+1)
	elseif (c == 'n') then
		if not json:match('^ull', pos+1) then
			error('unrecognised json value', elevel)
		end
		return null, pos+4
	elseif (c == 't') then
		if not json:match('^rue', pos+1) then
			error('unrecognised json value', elevel)
		end
		return true, pos+4
	elseif (c == 'f') then
		if not json:match('^alse', pos+1) then
			error('unrecognised json value', elevel)
		end
		return false, pos+5
	elseif c:match('[%d%-]') then
		local num, pos = strmatch(json,'^([%d%.%-eE%+]+)()',pos)
		num = tonumber(num)
		if not num then
			error('unrecognised json value', elevel)
		end
		return num,pos
	else
		error('unrecognised json value', elevel)
	end
end

function decode(json)
	if isustring(json) then
	else
		json = tostring(json)
		local pos = strfind(json, '%S')
		if not pos then
			error('cannot decode completely empty string as json',2)
		end
		local value; value, pos = jsonvalueat(json, pos, 3)
		if strfind(json, '%S', pos) then
			error('unexpected content after json value',2)
		end
		if (value == null) then
			-- json.null only useful in objects and arrays
			return nil
		end
		return value
	end
end


