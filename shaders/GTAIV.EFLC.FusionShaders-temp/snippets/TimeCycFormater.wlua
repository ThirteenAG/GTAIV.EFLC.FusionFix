-----------------------------------------------------------------------------------------------
-- Name = Timecyc Formater
-- Function = Formats the timecyc file, correctly aligning values with titles for easy editing.
-- Version = 0.0.1 Test
-- File = Timecyc_Formater.wlua
-- Date = 01/08/2023
-- EditDate = 02/08/2023
-- Autor = RaphaelK12
-- Lauch = ??/??/????
-----------------------------------------------------------------------------------------------


require( "iuplua" )
require( "iupluaim" )
require( "iupluacontrols" )

local max = math.max

local vars = {
	"Ambient0Color_RGB",
	"Ambient1Color_RGB",
	"DirLightColor_RGB",
	"unusedParam",
	"FilmGrain",
	"SkyBottomColorFogDensity_RGBA",
	"SunCore_RGB",
	"SunCorona_RGB",
	"SunSz",
	"CoronaBrightness",
	"FarClip",
	"FogStart",
	"LowCloudsColor_RGB",
	"BottomCloudsColor_RGB",
	"Water_RGBA",
	"Exposure",
	"BloomThreshold",
	"MidGrayValue",
	"BloomIntensity",
	"ColorCorrection_RGB",
	"ColorAdd_RGB",
	"Desaturation",
	"Contrast",
	"Gamma",
	"DesaturationFar",
	"ContrastFar",
	"GammaFar",
	"DepthFxNear",
	"DepthFxFar",
	"LumMin",
	"LumMax",
	"LumDelay",
	"CloudAlpha",
	"DirLightMultiplier",
	"Ambient0Multiplier",
	"Ambient1Multiplier",
	"SkyLightMultiplier",
	"DirLightSpecMultiplier",
	"Temperature",
	"GlobalReflectionMultiplier",
	"SkyColor_RGB",
	"SkyHorizonColor_RGB",
	"SkyEastHorizonColor_RGB",
	"Cloud1Color_RGB",
	"Unknown3",
	"SkyHorizonHeight",
	"SkyHorizonBrightness",
	"unusedParam",
	"unusedParam",
	"Cloud2Color_RGB",
	"Cloud2ShadowStrength",
	"Cloud2Threshold",
	"Cloud2Bias1",
	"Cloud2Scale",
	"unusedParam",
	"Cloud2Bias2",
	"DetailNoiseScale",
	"DetailNoiseMultiplier",
	"Cloud2Offset",
	"unusedParam",
	"CloudsFadeOut",
	"Cloud1Bias",
	"Cloud1Detail",
	"Cloud1Threshold",
	"Cloud1Height",
	"Cloud3Color_RGB",
	"Unknown29",
	"SunColor_RGB",
	"CloudsBrightness",
	"DetailNoiseOffset",
	"StarsBrightness",
	"VisibleStars",
	"MoonBrightness",
	"MoonColor_RGB",
	"MoonGlow",
	"MoonParam3",
	"unusedParam",
	"unusedParam",
	"SunSize",
	"SunAttenuation",
	"DOFStart",
	"unusedParam",
	"unusedParam",
	"NearDOFBlur",
	"FarDOFBlur",
	"WaterReflectionMultiplier",
	"ParticleBrightness",
	"CoronaSize",
	"SkyBrightness",
	"AOStrength",
	"RimLightingMultiplier",
	"DistantCoronaBrightness",
	"DistantCoronaSize",
	"PedAOStrength",
	title = true
}

function backupFile(filename)
	local backupfilename = ""
	local backupfile = false
	for i = 0, 10 do
		if i == 0 then
			backupfilename = filename .. ".bak"
		elseif i == 10 then
			backupfilename = filename .. ".bak" .. tostring(i)
		else
			backupfilename = filename .. ".bak" .. tostring(i)
		end
		backupfile = io.open(backupfilename, "r")
		if not backupfile then
			local ret = os.execute(string.format('copy "%s" "%s"', filename, backupfilename))
			return ret
		else
			backupfile:close()
		end
	end
end

function Formater(filename)
	local file = io.open(filename, "r")
	if not file then
		return nil
	end
	local lines = file:lines()
	local parsedFile = {}
	local currentTitle = {}
	local currentValues = {value = true, var = {}}
	local linesValues = {}
	for line in lines do
		if (line == "" or line == "\n" or line == " " )then
			table.insert(parsedFile, "\n")
		elseif(line:sub(1, 5):upper() == "//AMB" ) then		-- title (value names)
			table.insert(parsedFile, line)
			table.insert(parsedFile, "\n")
			table.insert(parsedFile, vars)
			table.insert(parsedFile, "\n")
		elseif(line:sub(1, 2) == "//" )then 				-- comments
			table.insert(parsedFile, line)
			table.insert(parsedFile, "\n")
		else 												-- values
			it = string.gmatch (line, "(%-?%d+%.?%d*)")
			for i in it do
				table.insert(currentValues, i)
			end
			table.insert(parsedFile, currentValues)
			table.insert(linesValues, currentValues)
			table.insert(parsedFile, "\n")
			currentValues = {value = true, var = {}}
		end
	end
	file:close()
	-- get max sizes for each value
	local maxSizes = {}
	for _, line in ipairs(linesValues) do
		if type(line) == "table" and line.value then
			for i, var in ipairs(line) do
				if type(var) == "string" then
					if(maxSizes[i]) then
						maxSizes[i] = max(maxSizes[i], #var)
					else
						maxSizes[i] = #var
					end
				end
			end
		end
	end
	-- add spaces to resize
	for _, line in ipairs(linesValues) do
		if type(line) == "table" and line.value then
			for i, var in ipairs(line) do
				if type(var) == "string" then
					local sz = maxSizes[i] - #var
					if(sz>0) then
						line[i] = line[i] .. string.rep(" ", sz)
					end
				end
			end
		end
	end
	-- pack values for rgb, rgba
	local cnt = 1
	local cnt2 = 1
	for i, name in ipairs(vars) do
		local sz = 0
		for j, line in ipairs(linesValues) do
			if string.find(name, "RGBA") then     -- 4
				table.insert(line.var, 	line[cnt+0] .. " " .. line[cnt+1] .. " " .. line[cnt+2] .. " " .. line[cnt+3])
			elseif string.find(name, "RGB") then  -- 3
				table.insert(line.var, 	line[cnt+0] .. " " .. line[cnt+1] .. " " .. line[cnt+2] )
			else								  -- 1
				table.insert(line.var, 	line[cnt+0]  )
			end
		end
		if string.find(name, "RGBA") then     -- 4
			cnt = cnt + 4
		elseif string.find(name, "RGB") then  -- 3
			cnt = cnt + 3
		else								  -- 1
			cnt = cnt + 1
		end
	end
	-- resize linesValues.var
	for i, name in ipairs(vars) do
		local sz = 0
		for j, line in ipairs(linesValues) do
			if(#name > #line.var[i] ) then
				sz = #name - #line.var[i]
				line.var[i] = line.var[i] .. string.rep(" ", sz)
			end
		end
	end
	-- resize vars
	for i, var in ipairs(linesValues[1].var) do
		local sz = 0
		if( #var > #vars[i]) then
			sz = #var - #vars[i]
			vars[i] = vars[i] .. string.rep(" ", sz)
		end
	end

	local ret = backupFile(filename) -- make a backup first
	if ret and ret == 0 then
		local file = io.open(filename, "w+") -- reopen file to write
		if not file then
			return nil
		end
		-- write file
		for k, v in ipairs(parsedFile) do
			if(type(v) == "string") then
				file:write(v)
			elseif(type(v) == "table") then
				if(v.title == true) then			-- title
	-- 				io.stdout:write("//")
					file:write("//")
					for i, j in ipairs(v) do
	-- 					io.stdout:write(j, "    ")
						file:write(j, "    ")
					end
				elseif (v.value ) then				-- value
					for i, j in ipairs(v.var) do
						if(type(j) == "string" ) then
	-- 						io.stdout:write(j, "    ")
							file:write(j, "    ")
							if(i==1) then
	-- 							io.stdout:write("  ")
								file:write("  ")
							end
						end
					end
				end
			end
		end
		file:close()
	end
end

files = {};

dlg_TimecycFormater=iup.dialog{
	title = 'Timecyc Formater',
	minsize = '300x150',
	size = '200x80',
	maxsize = '800x600',
	iup.vbox{
		iup.fill{},
		iup.hbox{
			iup.fill{},
			iup.label {title="Drop timecyc file here!!", font = "COURIER_BOLD_14"},
			iup.fill{},
			expand = "horizontal",
			alignment = "ACENTER",
		},
		iup.fill{},
		alignment = "ACENTER",
		bgcolor = "150 150 150",
	};
};

function dlg_TimecycFormater:dropfiles_cb(filename, num)
-- 	print(num, filename)
	table.insert(files, filename)
	if num == 0 then
-- 		print()
		for k, file in ipairs(files) do
			Formater(file)
		end
		files = {}
-- 		table.insert(files, "")
	end
end

dlg_TimecycFormater:show()

if (iup.MainLoopLevel()==0) then
	iup.MainLoop()
end














