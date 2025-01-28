/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file script_types.hpp Defines all the types of the game, like IDs of various objects.
 */

/**
 * @page script_ids Identifying game object
 * IDs are used to identify certain objects. They are only unique within the object type, so for example a vehicle may have VehicleID 2009,
 * while a station has StationID 2009 at the same time. Also IDs are assigned arbitrary, you cannot assume them to be consecutive.
 * Also note that some IDs are static and never change, while others are allocated dynamically and might be
 * reused for other objects once they are released. So be careful, which IDs you store for which purpose and whether they stay valid all the time.
 *
 * <table>
 * <tr><th>type         </th><th> object                                            </th>
 *                           <th> acquired                                          </th>
 *                           <th> released                                          </th>
 *                           <th> reused                                            </th></tr>
 * <tr><td>#BridgeType  </td><td> bridge type                                       </td>
 *                           <td> introduction \ref newgrf_changes "(1)"            </td>
 *                           <td> never \ref newgrf_changes "(1)"                   </td>
 *                           <td> no \ref newgrf_changes "(1)"                      </td></tr>
 * <tr><td>#CargoType   </td><td> cargo type                                        </td>
 *                           <td> game start \ref newgrf_changes "(1)"              </td>
 *                           <td> never \ref newgrf_changes "(1)"                   </td>
 *                           <td> no \ref newgrf_changes "(1)"                      </td></tr>
 * <tr><td>#ClientID    </td><td> network client (player)                           </td>
 *                           <td> joining server                                    </td>
 *                           <td> leaving server                                    </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#CompanyID   </td><td> company                                           </td>
 *                           <td> launch                                            </td>
 *                           <td> merger, bankruptcy                                </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#EngineID    </td><td> engine type                                       </td>
 *                           <td> introduction, preview \ref dynamic_engines "(2)"  </td>
 *                           <td> engines retires \ref dynamic_engines "(2)"        </td>
 *                           <td> no \ref dynamic_engines "(2)"                     </td></tr>
 * <tr><td>#GoalID      </td><td> goal                                              </td>
 *                           <td> creation                                          </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#GroupID     </td><td> vehicle group                                     </td>
 *                           <td> creation                                          </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#IndustryID  </td><td> industry                                          </td>
 *                           <td> construction                                      </td>
 *                           <td> closure                                           </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#IndustryType</td><td> industry type                                     </td>
 *                           <td> game start \ref newgrf_changes "(1)"              </td>
 *                           <td> never \ref newgrf_changes "(1)"                   </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#LeagueTableID</td><td> league table                                     </td>
 *                           <td> creation                                          </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#LeagueTableElementID</td><td> element of a league table                 </td>
 *                           <td> creation                                          </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#ObjectType  </td><td> NewGRF object type                                </td>
 *                           <td> game start \ref newgrf_changes "(1)"              </td>
 *                           <td> never \ref newgrf_changes "(1)"                   </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#ScriptErrorType</td><td> error message                                  </td>
 *                           <td> OpenTTD start \ref transient_id "(3)"             </td>
 *                           <td> OpenTTD exit                                      </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#SignID      </td><td> sign                                              </td>
 *                           <td> construction                                      </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#StationID   </td><td> station                                           </td>
 *                           <td> construction                                      </td>
 *                           <td> expiration of 'grey' station sign after deletion  </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#StringID    </td><td> translatable text                                 </td>
 *                           <td> OpenTTD start \ref transient_id "(3)"             </td>
 *                           <td> OpenTTD exit                                      </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#SubsidyID   </td><td> subsidy                                           </td>
 *                           <td> offer announcement                                </td>
 *                           <td> (offer) expiration                                </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#StoryPageID </td><td> story page                                        </td>
 *                           <td> creation                                          </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#StoryPageElementID</td><td> story page element                          </td>
 *                           <td> creation                                          </td>
 *                           <td> deletion                                          </td>
 *                           <td> yes                                               </td></tr>
 * <tr><td>#TileIndex   </td><td> tile on map                                       </td>
 *                           <td> game start                                        </td>
 *                           <td> never                                             </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#TownID      </td><td> town                                              </td>
 *                           <td> game start                                        </td>
 *                           <td> never                                             </td>
 *                           <td> no                                                </td></tr>
 * <tr><td>#VehicleID   </td><td> vehicle                                           </td>
 *                           <td> construction, autorenew, autoreplace              </td>
 *                           <td> destruction, autorenew, autoreplace               </td>
 *                           <td> yes                                               </td></tr>
 * </table>
 *
 * @remarks
 *  \li \anchor newgrf_changes  (1) in-game changes of newgrfs may reassign/invalidate IDs (will also cause other trouble though).
 *  \li \anchor dynamic_engines (2) engine IDs are reassigned/invalidated on changing 'allow multiple newgrf engine sets' (only allowed as long as no vehicles are built).
 *  \li \anchor transient_id    (3) string/error IDs are only valid during a session, and may be reassigned/invalidated when loading savegames (so you cannot store them).
 */

#ifndef SCRIPT_TYPES_HPP
#define SCRIPT_TYPES_HPP

#include <squirrel.h>

#ifdef DOXYGEN_API
/* Define all types here, so they are added to the API docs. */
using BridgeType = uint32_t; ///< The ID of a bridge type.
using CargoType = uint8_t; ///< The ID of a cargo type.
using ClientID = uint32_t; //< The ID of a (network) client.
using CompanyID = uint8_t; ///< The ID of a company.
using EngineID = uint16_t; ///< The ID of an engine.
using GoalID = uint16_t; ///< The ID of a goal.
using GroupID = uint16_t; ///< The ID of a group.
using IndustryID = uint16_t; ///< The ID of an industry.
using IndustryType = uint8_t; ///< The ID of an industry-type.
using Money = int64_t; ///< Money, stored in a 32bit/64bit safe way. For scripts money is always in pounds.
using LeagueTableID = uint8_t; ///< The ID of a league table.
using LeagueTableElementID = uint16_t; ///< The ID of an element of a league table.
using ObjectType = uint16_t; ///< The ID of an object-type.
using SignID = uint16_t; ///< The ID of a sign.
using StationID = uint16_t; ///< The ID of a station.
using StringID = uint32_t; ///< The ID of a string.
using SubsidyID = uint16_t; ///< The ID of a subsidy.
using StoryPageID = uint16_t; ///< The ID of a story page.
using StoryPageElementID = uint16_t; ///< The ID of a story page element.
using TileIndex = uint32_t; ///< The ID of a map location.
using TownID = uint16_t; ///< The ID of a town.
using VehicleID = uint32_t; ///< The ID of a vehicle.
#endif /* DOXYGEN_API */

/**
 * The types of errors inside the script framework.
 *
 * Possible value are defined inside each API class in an ErrorMessages enum.
 */
using ScriptErrorType = uint32_t;

#endif /* SCRIPT_TYPES_HPP */
