import skse;
import Shared.GlobalFunc;

class FactionRanks extends MovieClip {

    public static var instance;

    public var Menu_mc:MovieClip;
    public var StatsPage_mc:MovieClip;
    public var CategoryList:MovieClip;
    public var StatsList_mc:MovieClip;

    public var itemMargin:Number = 40;

    private var rendered:Boolean = false;

    function FactionRanks() {
        FactionRanks.instance = this;
    }

    function onLoad() {
        _visible = false;
        Shared.GlobalFunc.MaintainTextFormat();
        Menu_mc = _parent._parent.QuestJournalFader.Menu_mc;
        StatsPage_mc = Menu_mc.StatsFader.Page_mc;
        CategoryList = StatsPage_mc.CategoryList;
        StatsList_mc = StatsPage_mc.StatsList_mc;
        CategoryList.entryList = [ { text : '$FACR_TITLE', stats: new Array(), isFactionRanks : true } ].concat( CategoryList.entryList );
        CategoryList.InvalidateData();

        StatsPage_mc.FR__onCategoryHighlight = StatsPage_mc.onCategoryHighlight;
        StatsPage_mc.onCategoryHighlight = onCategoryHighlight;
        Menu_mc.TabButtonGroup.addEventListener("itemClick", this, "onTabClick");
    }

    function onCategoryHighlight() {
        this = FactionRanks.instance;
        if ( CategoryList.selectedEntry.isFactionRanks ) {
            StatsList_mc._visible = false;
            render();
        } else {
            _visible = false;
            StatsList_mc._visible = true;

            var stats: Array = CategoryList.EntriesA[CategoryList.iSelectedIndex - 1].stats;
            StatsList_mc.ClearList();
            StatsList_mc.scrollPosition = 0;
            
            for (var i: Number = 0; i < stats.length; i++) {
                StatsList_mc.entryList.push(stats[i]);
            }
            StatsList_mc.InvalidateData();
        }
    }

    function onTabClick(event: Object): Void {
        if (Menu_mc.bTabsDisabled) {
            return;
        }

        if ( event.item == Menu_mc.StatsTab && CategoryList.selectedEntry.isFactionRanks ) {
            render();
        } else {
            _visible = false;
        }
    }

    function render() {
        _visible = true;
        if (rendered) return;
        rendered = true;

        var y = 0;
        for (var i = 0; i < _root._FactionRanks.length; i++) {
            var data:Object = _root._FactionRanks[i];
            var mc = attachMovie("Item", 'item_' + i, getNextHighestDepth());
            mc.Label_tf.SetText(data.label + data.value);
            mc.Knot_mc.attachMovie(data.id, 'knot', mc.Knot_mc.getNextHighestDepth());
            mc._y = y;
            y += mc._height + itemMargin;
        }
    }
}