local oldHwnd = nil

function CloseOldPage()
	local ApiUtil = XLGetObject("API.Util")
	if oldHwnd then
		local WM_CLOSE = 0x0010
		--必须发送两次， 第一次关闭浏览器标签， 第二次关闭进程
		ApiUtil:SendMessage(oldHwnd, WM_CLOSE, 0, 0)
		ApiUtil:SendMessage(oldHwnd, WM_CLOSE, 0, 0)
	end
end

--lua文件必须是UTF-8编码的(最好无BOM头)
function close_btn_OnLButtonDown(self)
	local owner = self:GetOwner()
	CloseOldPage()
	os.exit()
end

function OnInitControl(self)
	local owner = self:GetOwner()
	--注册回调
	local ApiUtil = XLGetObject("API.Util")
	if ApiUtil and type(ApiUtil.AttachListener) == "function" then
		ApiUtil:AttachListener(function(hwnd)
			local browserobj = owner:GetUIObject("main.browser")
			if browserobj then
				CloseOldPage()
				browserobj:SetWindow(hwnd)
				oldHwnd = hwnd
			end
		end)
		--[[local hwnd = ApiUtil:FindWindow("", "计算器")
		local browserobj = owner:GetUIObject("main.browser")
		XLMessageBox(tostring(hwnd))
		if hwnd then
			browserobj:SetWindow(hwnd)
		end]]
	end
end

function MSG_OnMouseMove(self)
	self:SetTextFontResID ("msg.font.bold")
	self:SetCursorID ("IDC_HAND")
end

function MSG_OnMouseLeave(self)
	self:SetTextFontResID ("msg.font")
	self:SetCursorID ("IDC_ARROW")
end

function userdefine_btn_OnClick(self)
	XLMessageBox("Click on button")
end
