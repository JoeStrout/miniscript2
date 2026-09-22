-- md2pdf-toc.lua -- build a table of contents and place it after the lede.
--
-- Pandoc's own --toc can only put the TOC at the very top of the document,
-- ahead of the opening paragraphs.  This filter instead collects the level-2
-- and level-3 headings itself and inserts the list immediately before the
-- first level-2 heading, so the lede still comes first.
--
-- The result is a <div class="toc">; tools/md2pdf.css styles it, and uses
-- WeasyPrint's target-counter() to print the page number of each entry.

local function toc_link(header)
	-- Strip any footnotes/links out of the heading text before reusing it.
	local label = pandoc.utils.stringify(header.content)
	return pandoc.Link(pandoc.Str(label), '#' .. header.identifier)
end

function Pandoc(doc)
	local entries = {}
	local first_h2 = nil

	for i, block in ipairs(doc.blocks) do
		if block.t == 'Header' and block.identifier ~= '' then
			if block.level == 2 then
				if not first_h2 then first_h2 = i end
				entries[#entries + 1] = {level = 2, link = toc_link(block), subs = {}}
			elseif block.level == 3 and #entries > 0 then
				local parent = entries[#entries]
				parent.subs[#parent.subs + 1] = toc_link(block)
			end
		end
	end

	if not first_h2 or #entries == 0 then return doc end

	-- Top-level list, each item optionally carrying a nested list of its h3s.
	local items = {}
	for _, e in ipairs(entries) do
		local item = {pandoc.Plain({e.link})}
		if #e.subs > 0 then
			local subitems = {}
			for _, link in ipairs(e.subs) do
				subitems[#subitems + 1] = {pandoc.Plain({link})}
			end
			item[#item + 1] = pandoc.BulletList(subitems)
		end
		items[#items + 1] = item
	end

	local toc = pandoc.Div(
		{
			pandoc.Div({pandoc.Plain({pandoc.Str('Contents')})}, pandoc.Attr('', {'toc-title'})),
			pandoc.BulletList(items),
		},
		pandoc.Attr('', {'toc'})
	)

	table.insert(doc.blocks, first_h2, toc)
	return doc
end
