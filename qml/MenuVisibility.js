.pragma library

// Keep the declared controls so restoring a type preserves its action and menu position.
function capture(menu) {
    const entries = []
    for (let i = 0; i < menu.count; ++i) {
        const item = menu.itemAt(i)
        const group = menu.menuAt(i)
        entries.push({ control: group || item, type: item.adjustmentType || "", group: !!group,
                       separator: !group && item.text === undefined,
                       children: group ? capture(group) : [] })
    }
    return entries
}

function choices(entries) {
    const sections = []
    // Relighting also has a 3D shortcut. List it once with the other lighting controls.
    for (const entry of entries) {
        if (entry.type) sections.push({ group: null, choices: [entry] })
        if (entry.group && entry.children.every(child => child.type)) {
            sections.push({ group: entry.control, choices: entry.children })
        }
    }
    return sections
}

function apply(menu, entries, hidden) {
    let index = 0
    let separatorAllowed = false
    const wanted = entries.map(entry => {
        if (entry.group) apply(entry.control, entry.children, hidden)
        return !entry.separator && (!entry.type || hidden.indexOf(entry.type) < 0)
                && (!entry.group || entry.control.count > 0)
    })
    for (let i = 0; i < entries.length; ++i) {
        const entry = entries[i]
        const show = entry.separator ? separatorAllowed && wanted.indexOf(true, i + 1) >= 0 : wanted[i]
        const present = index < menu.count &&
                (entry.group ? menu.menuAt(index) : menu.itemAt(index)) === entry.control
        if (show) {
            if (!present) {
                if (entry.group) menu.insertMenu(index, entry.control)
                else menu.insertItem(index, entry.control)
            }
            ++index
            separatorAllowed = !entry.separator
        } else if (present) {
            if (entry.group) menu.takeMenu(index)
            else menu.takeItem(index)
        }
    }
}
