//
// Created by helmut on 13.03.18.
//

#ifndef COROUT_BOOKVIEW_H
#define COROUT_BOOKVIEW_H

#include <functional>

#include "Utils/FixTypes.h"
#include "Utils/CurrencyPair.h"

#include "Quote.h"

namespace CORE {
namespace BOOK {

class OrderBook;

/*! \brief Abstract base class for all book view classes (e.g. SortView and FilterView) */
class BookView
{
public:
	/*! \brief Type alias for raw pointer to BookView */
	using RawPtr = BookView *;
	/*! \brief Type alias for raw pointer to const BookView */
	using RawPtrToConst = const BookView *;
	/*! \brief Type alias for shared pointer to BookView */
	using Ptr = std::shared_ptr<BookView>;
	/*! \brief Type alias for shared pointer to const BookView */
	using PtrToConst = std::shared_ptr<const BookView>;
	/*! Type alias for pair of book view pointers */
	using ViewPair = UTILS::BidAskPair<BookView::Ptr>;
	
	/*! \brief Constructor
	 *
	 * @param name      Name of the view
	 * @param active    \a true -> the view is active, \a false -> the view is
	 * to be used only as an intermediary for constructing other views
	 * */
	explicit BookView(std::string name, bool active = true)
			: m_name(std::move(name)), m_active(active) { }
	
	/**
	 * Is this BookView valid?
	 *
	 * Pure virtual function, to be overriden in derived classes.
	 *
	 * @return @a true if the BookView is valid.
	 */
	virtual bool Valid() const = 0;

	/*! \brief Name of the view */
	const std::string &Name() const { return m_name; }
	
	/*! \brief View is active */
	bool Active() const { return m_active; }
	
	/*! \brief Set view to active or inactive */
	void SetActive(bool active) { m_active = active; }
	
	/*! \brief Type alias for vector of quote groups */
	using QuoteGroupVec = std::vector<QuoteGroup::Ptr>;
	/*! \brief Type alias for functions on quote groups */
	using QuoteGroupFunc = std::function<void(int, QuoteGroup::Ptr &, bool &)>;
	/*! \brief Type alias for predicates on quotes */
	using QuotePred = std::function<bool(const Quote &)>;
	
	QuoteGroupVec GetLevels(unsigned int n = 0, const QuotePred &quotePred = nullptr) const;
	
	void GetLevels(QuoteGroupVec &vec, unsigned int n = 0, const QuotePred &quotePred = nullptr) const;
	
	QuoteGroup::Ptr AggregateLevel(int64_t volume, const QuotePred &quotePred = nullptr) const;
	
	void AggregateLevels(int64_t minVolumePerLevel, const QuoteGroupFunc &action, const QuotePred &quotePred = nullptr) const;

	/*! \brief Iterate quote groups.
	 *
	 * Pure virtual function to be overridden by derived classes.
	 *
	 * */
	virtual void IterateQuoteGroups(const QuoteGroupFunc &action, const QuotePred &quotePred = nullptr) const = 0;
	
	/*! \brief Pointer to the underlying OrderBook.
	 *
	 * Virtual function to be overridden by derived classes.
	 *
	 * */
	virtual const OrderBook *BookPtr() const { return nullptr; }
	
	/*! \brief Pointer to the direct source view.
	 *
	 * Virtual function to be overridden by derived classes.
	 *
	 * */
	virtual BookView::Ptr SourceViewPtr() { return nullptr; }
	
	/*! \brief Const Pointer to the direct source view.
	 *
	 * Virtual function to be overridden by derived classes.
	 *
	 * */
	virtual BookView::PtrToConst SourceViewPtr() const { return nullptr; }
	
	BookView::RawPtr SortViewPtr();
	
	BookView::RawPtrToConst SortViewPtr() const;
	
	std::string GetFilterSequence(bool skipSortView = false, const std::string &delimiter = " -> ") const;
	
	/*! \brief Append filter condition as string.
	 *
	 * Pure virtual function to be overridden by derived classes.
	 *
	 * */
	virtual void AppendFilter(std::stringstream &ss, bool skipSortView, const std::string &delimiter) const = 0;
	
	
	/*! \brief Instrument.
	 *
	 * Pure virtual function to be overridden by derived classes.
	 *
	 * */
	virtual UTILS::CurrencyPair Instrument() const = 0;
	
	/*! \brief Side (bid/ask).
	 *
	 * Pure virtual function to be overridden by derived classes.
	 *
	 * */
	virtual UTILS::QuoteType Type() const = 0;
	
	bool Bid() const { return Type().Bid(); }
	
	/*! \brief Side as string ("bid", "ask") */
	std::string TypeString() const { return Bid() ? "bid" : "ask"; }
	
	bool Matches(UTILS::CurrencyPair instrument, UTILS::Side side) const;


private:
	/*! \brief Name of the view */
	const std::string m_name;
	
	/*! \brief View is active (or intermediary) */
	bool m_active;
	
};

} // namespace BOOK
} // namespace CORE


#endif //COROUT_BOOKVIEW_H
